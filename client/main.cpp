#include <stdio.h>	    
#include <fcntl.h>
#include <io.h>	  
#include <vector>
#include "stdinput.h"
#include "launch.h"
#include "attach.h"
#include "server.h"
#include "dbg_capabilities.h"
#include <base/util/unicode.h>
#include <base/util/unicode.cpp>
#include <base/filesystem.h>

bool create_process_with_debugger(vscode::rprotocol& req, uint16_t port);

uint16_t server_port = 0;
int64_t seq = 1;

void response_initialized(stdinput& io, vscode::rprotocol& req)
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

int stoi_nothrow(std::string const& str)
{
	try {
		return std::stoi(str);
	}
	catch (...) {
	}
	return 0;
}

int main()
{
	_setmode(_fileno(stdout), _O_BINARY);
	setbuf(stdout, NULL);

	stdinput io;
	vscode::rprotocol initproto;
	vscode::rprotocol connectproto;
	std::unique_ptr<attach> attach_;
	std::unique_ptr<launch> launch_;
	std::unique_ptr<server> server_;

	for (;; std::this_thread::sleep_for(std::chrono::milliseconds(10))) {
		if (launch_) {
			launch_->update();
			continue;
		}
		if (attach_) {
			attach_->update();
			continue;
		}
		if (server_) {
			server_->update();
			continue;
		}
		while (!io.input_empty()) {
			vscode::rprotocol rp = io.input();
			if (rp["type"] != "request") {
				continue;
			}
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
					if (!server_) {
						server_.reset(new server("127.0.0.1", 0));
						server_->event_recv([&](const std::string& msg) {
							uint16_t port = stoi_nothrow(msg);
							if (!port) {
								response_error(io, connectproto, "Launch failed");
								exit(0);
								return;
							}
							attach_.reset(new attach(io));
							attach_->connect(net::endpoint("127.0.0.1", port));
							if (seq > 1) initproto.AddMember("__initseq", seq, initproto.GetAllocator());
							attach_->send(initproto);
							attach_->send(connectproto);
						});
						server_port = wait_ok(server_.get());
						if (!server_port) {
							response_error(io, rp, "Launch failed");
							exit(0);
							continue;
						}
					}
					if (!create_process_with_debugger(rp, server_port)) {
						response_error(io, rp, "Launch failed");
						exit(0);
						continue;
					}
					connectproto = std::move(rp);
				}
				else {
					launch_.reset(new launch(io));
					if (!launch_->request_launch(rp)) {
						response_error(io, rp, "Launch failed");
						exit(0);
						continue;
					}
					if (seq > 1) initproto.AddMember("__initseq", seq, initproto.GetAllocator());
					launch_->send(std::move(initproto));
					launch_->send(std::move(rp));
					break;
				}
			}
			else if (rp["command"] == "attach") {
				attach_.reset(new attach(io));
				auto& args = rp["arguments"];
				std::string ip = args.HasMember("ip") ? args["ip"].Get<std::string>() : "127.0.0.1";
				uint16_t port = args.HasMember("port") ? args["port"].GetUint() : 4278;
				attach_->connect(net::endpoint(ip, port));
				if (seq > 1) initproto.AddMember("__initseq", seq, initproto.GetAllocator());
				attach_->send(initproto);
				attach_->send(rp);
			}
		}
	}
	io.join();
}
