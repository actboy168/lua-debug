#include <stdio.h>	    
#include <fcntl.h>
#include <io.h>	  
#include <vector>
#include <thread>
#include <net/tcp/connecter.h>
#include <net/poller.h>
#include <rapidjson/document.h>
#include "dbg_protocol.h"	 
#include "dbg_format.h"	 
#include "dbg_unicode.h"
#include "launch.h"
#include "dbg_capabilities.h"
#include <base/hook/fp_call.h>

using namespace vscode;

class stdinput
	: public std::thread
{
public:	
	stdinput()
		: std::thread(std::bind(&stdinput::run, this))
	{ }

	void run() {
		for (;;) {
			while (update()) {}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	bool update() {
		char buf[32] = { 0 };
		if (fgets(buf, sizeof buf - 1, stdin) == NULL) {
			return false;
		}
		buf[sizeof buf - 1] = 0;
		if (strncmp(buf, "Content-Length: ", 16) != 0) {
			return false;
		}
		char *bufp = buf + 16;
		int len = atoi(bufp);
		if (len <= 0) {
			exit(1);
		}
		if (fgets(buf, sizeof buf, stdin) == NULL) {
			exit(1);
		}
		buffer_.resize(len + 1);
		buffer_[0] = 0;
		if (fread(buffer_.data(), len, 1, stdin) != 1) {
			exit(1);
		}
		buffer_[len] = 0;
		rapidjson::Document	d;
		if (d.Parse(buffer_.data(), len).HasParseError())  {
			exit(1);
		}
		input_queue_.push(rprotocol(std::move(d)));
		return true;
	}
	bool empty() const {
		return input_queue_.empty();
	}
	rprotocol pop() {
		rprotocol r = std::move(input_queue_.front());
		input_queue_.pop();
		return r;
	}
	static void output(const char* str, size_t len) {
		for (;;) {
			size_t r = fwrite(str, len, 1, stdout);
			if (r == 1)
				break;
		}
	}
	static void output(const wprotocol& wp) {
		if (!wp.IsComplete())
			return;
		auto l = format("Content-Length: %d\r\n\r\n", wp.size());
		output(l.data(), l.size());
		output(wp.data(), wp.size());
	}

private:
	std::vector<char> buffer_;
	net::queue<rprotocol, 8> input_queue_;
};

class proxy_client
	: public net::tcp::connecter
{
	typedef net::tcp::connecter base_type;
public:
	proxy_client()
		: poller()
		, base_type(&poller)
	{
		net::socket::initialize();
	}

	bool event_in()
	{
		if (!base_type::event_in())
			return false; 
		std::vector<char> tmp(base_type::recv_size());
		size_t len = base_type::recv(tmp.data(), tmp.size());
		if (len == 0)
			return true;
		stdinput::output(tmp.data(), len);
		return true;
	}

	void send(const rprotocol& rp)
	{
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		rp.Accept(writer);
		base_type::send(format("Content-Length: %d\r\n\r\n", buffer.GetSize()));
		base_type::send(buffer.GetString(), buffer.GetSize());
	}

	void event_close()
	{
		base_type::event_close();
		exit(0);
	}

	void update()
	{
		poller.wait(1000, 0);
	}

private:
	net::poller_t poller;
};

class module {		
public:		
	module(const wchar_t* path)		
		: handle_(LoadLibraryW(path))
	{ }		

	~module()
	{ }

	HMODULE handle() const		
	{		
		return handle_;		
	}		
	
	intptr_t api(const char* name)		
	{		
		return (intptr_t)GetProcAddress(handle_, name);		
	}		
	
private:		
	HMODULE handle_;		
};

bool create_process_with_debugger(rprotocol& req)
{
	module dll(L"debugger-inject.dll");
	if (!dll.handle()) {
		return false;
	}
	intptr_t create_process = dll.api("create_process_with_debugger");
	intptr_t set_luadll = dll.api("set_luadll");
	if (!create_process || !set_luadll) {
		return false;
	}

	auto& args = req["arguments"];
	if (args.HasMember("luadll") && args["luadll"].IsString()) {
		std::wstring wluadll = u2w(args["luadll"].Get<std::string>());
		if (!base::c_call<bool>(set_luadll, wluadll.data(), wluadll.size())) {
			return false;
		}
	}

	if (!args.HasMember("runtimeExecutable") || !args["runtimeExecutable"].IsString()) {
		return false;
	}

	std::wstring wcommand = u2w(args["runtimeExecutable"].Get<std::string>());
	if (args.HasMember("runtimeArgs") && args["runtimeArgs"].IsString()) {
		// TODO:
	}
	std::wstring wcwd;
	if (args.HasMember("cwd") && args["cwd"].IsString()) {
		wcwd = u2w(args["cwd"].Get<std::string>());
	}

	if (!base::c_call<bool>(create_process, 4278, wcommand.c_str(), wcwd.c_str())) {
		return false;
	}
	return true;
}

int64_t seq = 1;

void response_initialized(rprotocol& req)
{
	wprotocol res;
	vscode::capabilities(res, req["seq"].GetInt64());
	stdinput::output(res);
}

void response_error(rprotocol& req, const char *msg)
{
	wprotocol res;
	for (auto _ : res.Object())
	{
		res("type").String("response");
		res("seq").Int64(seq++);
		res("command").String(req["command"]);
		res("request_seq").Int64(req["seq"].GetInt64());
		res("success").Bool(false);
		res("message").String(msg);
	}
	stdinput::output(res);
}

int main()
{
	_setmode(_fileno(stdout), _O_BINARY);
	setbuf(stdout, NULL);

	stdinput input;
	rprotocol initproto;
	std::unique_ptr<proxy_client> client;
	std::unique_ptr<launch_server> server;

	for (;;) {
		if (server) server->update();
		if (client) client->update();
		while (!input.empty()) {
			rprotocol rp = input.pop();
			if (rp["type"] == "request") {
				if (rp["command"] == "initialize") {
					response_initialized(rp);
					initproto = std::move(rp);
					initproto.AddMember("__norepl", true, initproto.GetAllocator());
					seq = 1;
					continue;
				}
				else if (rp["command"] == "launch") {
					std::string console;
					auto& args = rp["arguments"];
					if (args.HasMember("runtimeExecutable")) {
						if (!create_process_with_debugger(rp)) {
							response_error(rp, "Launch failed");
							exit(1);
							continue;
						}
						client.reset(new proxy_client);
						client->connect(net::endpoint("127.0.0.1", 4278));
						if (seq > 1) initproto.AddMember("__initseq", seq, initproto.GetAllocator());
						client->send(initproto);
					}
					else {
						if (args.HasMember("console")) {
							console = args["console"].Get<std::string>();
						}
						server.reset(new launch_server(console, [&]() {
							while (!input.empty()) {
								rprotocol rp = input.pop();
								server->send(std::move(rp));
							}
						}));
						if (seq > 1) initproto.AddMember("__initseq", seq, initproto.GetAllocator());
						server->send(std::move(initproto));
					}
				}
				else if (rp["command"] == "attach") {
					client.reset(new proxy_client);
					std::string ip = "127.0.0.1";
					uint16_t port = 4278;
					auto& args = rp["arguments"];
					if (args.HasMember("ip")) {
						ip = args["ip"].Get<std::string>();
					}
					if (args.HasMember("port")) {
						port = args["port"].GetUint();
					}
					client->connect(net::endpoint(ip, port));
					if (seq > 1) initproto.AddMember("__initseq", seq, initproto.GetAllocator());
					client->send(initproto);
				}
			}  
			if (server) server->send(std::move(rp));
			if (client) client->send(rp);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	input.join();
}
