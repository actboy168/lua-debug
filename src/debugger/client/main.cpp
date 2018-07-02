#include <stdio.h>	    
#include <fcntl.h>
#include <io.h>	  
#include <vector>
#include <debugger/client/stdinput.h>
#include <debugger/client/run.h>
#include <debugger/client/tcp_attach.h>
#include <debugger/capabilities.h>
#include <debugger/io/helper.h>
#include <debugger/io/namedpipe.h>
#include <base/util/unicode.h>
#include <base/util/format.h>
#include <base/filesystem.h>
#include <base/win/query_process.h>
#include <base/win/process_switch.h>

static void response_initialized(stdinput& io, vscode::rprotocol& req)
{
	vscode::wprotocol res;

	for (auto _ : res.Object())
	{
		res("type").String("response");
		res("seq").Int64(1);
		res("command").String("initialize");
		res("request_seq").Int64(req["seq"].GetInt64());
		res("success").Bool(true);
		for (auto _ : res("body").Object())
		{
			vscode::capabilities(res);
		}
	}
	vscode::io_output(&io, res);
}

static void response_error(stdinput& io, vscode::rprotocol& req, const char *msg)
{
	vscode::wprotocol res;
	for (auto _ : res.Object())
	{
		res("type").String("response");
		res("seq").Int64(2);
		res("command").String(req["command"]);
		res("request_seq").Int64(req["seq"].GetInt64());
		res("success").Bool(false);
		res("message").String(msg);
	}
	vscode::io_output(&io, res);
	exit(-1);
}

static int stoi_nothrow(std::string const& str)
{
	try {
		return std::stoi(str);
	}
	catch (...) {
	}
	return 0;
}

static void sleep() {
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

static int run_createprocess_then_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req)
{
	int pid = 0;
	if (!create_process_with_debugger(req, pid)) {
		response_error(io, req, "Create process failed");
		return -1;
	}
	auto port = base::format(L"vscode-lua-debug-%d", pid);
	if (!run_pipe_attach(io, init, req, port)) {
		response_error(io, req, "Launch failed");
		return -1;
	}
	return 0;
}

int run_terminal_then_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req)
{
	auto port = base::format(L"vscode-lua-debug-%d", GetCurrentProcessId());
	if (!create_terminal_with_debugger(io, req, port)) {
		response_error(io, req, "Launch failed");
		return -1;
	}
	if (!run_pipe_attach(io, init, req, port)) {
		response_error(io, req, "Launch failed");
		return -1;
	}
	return 0;
}

int run_luaexe_then_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req)
{
	auto port = base::format(L"vscode-lua-debug-%d", GetCurrentProcessId());
	if (!create_luaexe_with_debugger(io, req, port)) {
		response_error(io, req, "Launch failed");
		return -1;
	}
	if (!run_pipe_attach(io, init, req, port)) {
		response_error(io, req, "Launch failed");
		return -1;
	}
	return 0;
}

static int run_attach_process(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req, int pid)
{
	base::win::process_switch m(pid, L"attachprocess");
	if (!open_process_with_debugger(req, pid)) {
		response_error(io, req, base::format("Open process (id=%d) failed.", pid).c_str());
		return -1;
	}
	auto port = base::format(L"vscode-lua-debug-%d", pid);
	if (!run_pipe_attach(io, init, req, port)) {
		response_error(io, req, "Attach failed");
		return -1;
	}
	return 0;
}

static std::vector<int> get_process_name(const std::string& name)
{
	std::vector<int> res;
	std::wstring wname = base::u2w(name);
	std::transform(wname.begin(), wname.end(), wname.begin(), ::tolower);
	base::win::query_process([&](const base::win::SYSTEM_PROCESS_INFORMATION* info)->bool {
		std::wstring pname(info->ImageName.Buffer, info->ImageName.Length / sizeof(wchar_t));
		std::transform(pname.begin(), pname.end(), pname.begin(), ::tolower);
		if (pname == wname) {
			res.push_back((int)info->ProcessId);
		}
		return false;
	});
	return res;
}


static int run_tcp_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req)
{
	tcp_attach attach(io);
	auto& args = req["arguments"];
	std::string ip = args.HasMember("ip") ? args["ip"].Get<std::string>() : "127.0.0.1";
	uint16_t port = args.HasMember("port") ? args["port"].GetUint() : 4278;
	attach.connect(net::endpoint(ip, port));
	attach.send(init);
	attach.send(req);
	for (;; sleep()) {
		attach.update();
	}
	return 0;
}

int main()
{
	_setmode(_fileno(stdout), _O_BINARY);
	setbuf(stdout, NULL);

	stdinput io;
	vscode::rprotocol init;

	for (;; sleep()) {
		vscode::rprotocol req = vscode::io_input(&io);
		if (req.IsNull()) {
			continue;
		}
		if (req["type"] != "request") {
			return -1;
		}
		if (init.IsNull()) {
			if (req["command"] == "initialize") {
				response_initialized(io, req);
				init = std::move(req);
				init.AddMember("__norepl", true, init.GetAllocator());
			}
			else {
				response_error(io, req, "not initialized");
				return -1;
			}
		}
		else {
			if (req["command"] == "launch") {
				auto& args = req["arguments"];
				if (args.HasMember("runtimeExecutable")) {
					return run_createprocess_then_attach(io, init, req);
				}
				if (args.HasMember("console") 
					&& args["console"].IsString() 
					&& (args["console"] == "integratedTerminal" || args["console"] == "externalTerminal")
				) {
					return run_terminal_then_attach(io, init, req);
				}
				return run_luaexe_then_attach(io, init, req);
			}
			else if (req["command"] == "attach") {
				auto& args = req["arguments"];
				if (args.HasMember("processId")) {
					if (!args["processId"].IsInt()) {
						response_error(io, req, "Attach failed");
						return -1;
					}
					return run_attach_process(io, init, req, args["processId"].GetInt());
				}
				if (args.HasMember("processName")) {
					if (!args["processName"].IsString()) {
						response_error(io, req, "Attach failed");
						return -1;
					}
					auto processName = args["processName"].Get<std::string>();
					auto& pid = get_process_name(processName);
					if (pid.size() == 0) {
						response_error(io, req, base::format("Cannot found process `%s`.", processName).c_str());
						return -1;
					}
					if (pid.size() > 1) {
						response_error(io, req, base::format("There are %d processes `%s`.", pid.size(), processName).c_str());
						return -1;
					}
					return run_attach_process(io, init, req, pid[0]);
				}
				return run_tcp_attach(io, init, req);
			}
		}
	}
}
