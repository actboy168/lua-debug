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
#include <base/file/stream.cpp>
#include <base/filesystem.h>
#include "dbg_iohelper.h"
#include "dbg_iohelper.cpp"

bool create_process_with_debugger(vscode::rprotocol& req, uint16_t port);

uint16_t server_port = 0;

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

static uint16_t wait_ok(server& s) {
	uint16_t port = 0;
	for (int i = 0; i < 10; ++i) {
		port = s.get_port();
		if (port) {
			return port;
		}
		sleep();
		s.update();
	}
	return 0;
}

static int run_createprocess_then_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& _req)
{
	std::unique_ptr<attach> attach_;
	vscode::rprotocol req = std::move(_req);
	server server("127.0.0.1", 0);
	server.event_recv([&](const std::string& msg) {
		uint16_t port = stoi_nothrow(msg);
		if (!port) {
			response_error(io, req, "Launch failed");
			return;
		}
		attach_.reset(new attach(io));
		attach_->connect(net::endpoint("127.0.0.1", port));
		attach_->send(init);
		attach_->send(req);
	});
	uint16_t server_port = wait_ok(server);
	if (!server_port) {
		response_error(io, req, "Launch failed");
		return -1;
	}
	if (!create_process_with_debugger(req, server_port)) {
		response_error(io, req, "Launch failed");
		return -1;
	}
	for (;; sleep()) {
		if (attach_) {
			attach_->update();
		}
		else {
			server.update();
		}
	}
	return 0;
}

static int run_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req)
{
	attach attach(io);
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
				else {
					return run_launch(io, init, req);
				}
			}
			else if (req["command"] == "attach") {
				return run_attach(io, init, req);
			}
		}
	}
}
