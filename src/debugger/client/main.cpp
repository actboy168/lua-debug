#include <stdio.h>	    
#include <fcntl.h>
#include <io.h>	  
#include <vector>
#include <debugger/client/stdinput.h>
#include <debugger/client/run.h>
#include <debugger/client/tcp_attach.h>
#include <base/util/unicode.h>
#include <base/util/format.h>
#include <base/filesystem.h>
#include <debugger/capabilities.h>
#include <debugger/io/helper.h>
#include <debugger/io/namedpipe.h>

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
		vscode::capabilities(res);
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
	auto port = base::format(L"vscode-lua-debug-%d", GetCurrentProcessId());
	if (!create_process_with_debugger(req, port)) {
		response_error(io, req, "Launch failed");
		return -1;
	}

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
				else if (args.HasMember("console") 
					&& (args["console"] == "integratedTerminal" || args["console"] == "externalTerminal")) {
					return run_terminal_then_attach(io, init, req);
				}
				else {
					return run_luaexe_then_attach(io, init, req);
					//return run_launch(io, init, req);
				}
			}
			else if (req["command"] == "attach") {
				return run_tcp_attach(io, init, req);
			}
		}
	}
}
