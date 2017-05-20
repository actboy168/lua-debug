#include <stdio.h>	    
#include <fcntl.h>
#include <io.h>	  
#include <vector>
#include "dbg_unicode.h"
#include "stdinput.h"
#include "launch.h"
#include "attach.h"
#include "dbg_capabilities.h"
#include "dbg_unicode.cpp"
#include <base/filesystem.h>
#include <base/hook/fp_call.h>

class module {
public:
	module(const wchar_t* path) : handle_(LoadLibraryW(path)) { }
	~module() { }
	HMODULE handle() const { return handle_; }
	intptr_t api(const char* name) { return (intptr_t)GetProcAddress(handle_, name); }
private:		
	HMODULE handle_;
};

uint16_t create_process_with_debugger(vscode::rprotocol& req)
{
	module dll(L"debugger-inject.dll");
	if (!dll.handle()) {
		return 0;
	}
	intptr_t create_process = dll.api("create_process_with_debugger");
	intptr_t set_luadll = dll.api("set_luadll");
	if (!create_process || !set_luadll) {
		return 0;
	}

	auto& args = req["arguments"];
	if (args.HasMember("luadll") && args["luadll"].IsString()) {
		std::wstring wluadll = vscode::u2w(args["luadll"].Get<std::string>());
		if (!base::c_call<bool>(set_luadll, wluadll.data(), wluadll.size())) {
			return 0;
		}
	}
	
	if (!args.HasMember("runtimeExecutable") || !args["runtimeExecutable"].IsString()) {
		return 0;
	}

	std::wstring wapplication = vscode::u2w(args["runtimeExecutable"].Get<std::string>());
	std::wstring wcommand = L"\"" + wapplication + L"\"";
	if (args.HasMember("runtimeArgs") && args["runtimeArgs"].IsString()) {
		wcommand = wcommand + L" " + vscode::u2w(args["runtimeArgs"].Get<std::string>());
	}
	std::wstring wcwd;
	if (args.HasMember("cwd") && args["cwd"].IsString()) {
		wcwd = vscode::u2w(args["cwd"].Get<std::string>());
	}
	else {
		wcwd = fs::path(wapplication).remove_filename();
	}

	return base::c_call<uint16_t>(create_process, wapplication.c_str(), wcommand.c_str(), wcwd.c_str());
}

int64_t seq = 1;

void response_initialized(stdinput& io, vscode::rprotocol& req)
{
	vscode::wprotocol res;
	vscode::capabilities(res, req["seq"].GetInt64());
	io.output(res);
}

void response_error(stdinput& io, vscode::rprotocol& req, const char *msg)
{
	vscode::wprotocol res;
	for (auto _ : res.Object())
	{
		res("type").String("response");
		res("seq").Int64(seq++);
		res("command").String(req["command"]);
		res("request_seq").Int64(req["seq"].GetInt64());
		res("success").Bool(false);
		res("message").String(msg);
	}
	io.output(res);
}

int main()
{
	_setmode(_fileno(stdout), _O_BINARY);
	setbuf(stdout, NULL);

	stdinput io;
	vscode::rprotocol initproto;
	std::unique_ptr<attach> attach_;
	std::unique_ptr<launch> launch_;

	for (;; std::this_thread::sleep_for(std::chrono::milliseconds(10))) {
		if (launch_) {
			launch_->update();
			continue;
		}
		if (attach_) {
			attach_->update();
			continue;
		}
		while (!io.input_empty()) {
			vscode::rprotocol rp = io.input();
			if (rp["type"] == "request") {
				if (rp["command"] == "initialize") {
					response_initialized(io, rp);
					initproto = std::move(rp);
					initproto.AddMember("__norepl", true, initproto.GetAllocator());
					seq = 1;
					continue;
				}
				else if (rp["command"] == "launch") {
					auto& args = rp["arguments"];
					if (args.HasMember("runtimeExecutable")) {
						uint16_t port = create_process_with_debugger(rp);
						if (port == 0) {
							response_error(io, rp, "Launch failed");
							exit(0);
							continue;
						}
						attach_.reset(new attach(io));
						attach_->connect(net::endpoint("127.0.0.1", port));
						if (seq > 1) initproto.AddMember("__initseq", seq, initproto.GetAllocator());
						attach_->send(initproto);
						attach_->send(rp);
					}
					else {
						launch_.reset(new launch(io));
						if (seq > 1) initproto.AddMember("__initseq", seq, initproto.GetAllocator());
						launch_->send(std::move(initproto));
						rp.AddMember("__stdout", "print", rp.GetAllocator());
						launch_->send(std::move(rp));
						break;
					}
				}
				else if (rp["command"] == "attach") {
					attach_.reset(new attach(io));
					std::string ip = "127.0.0.1";
					uint16_t port = 4278;
					auto& args = rp["arguments"];
					if (args.HasMember("ip")) {
						ip = args["ip"].Get<std::string>();
					}
					if (args.HasMember("port")) {
						port = args["port"].GetUint();
					}
					attach_->connect(net::endpoint(ip, port));
					if (seq > 1) initproto.AddMember("__initseq", seq, initproto.GetAllocator());
					attach_->send(initproto);
					attach_->send(rp);
				}
			}
		}
	}
	io.join();
}
