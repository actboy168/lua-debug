#pragma once

#include <stdint.h>
#include <functional>
#include <string>
#include <thread>

class server_impl;

class server
{
public:
	typedef std::function<void(const std::string&)> fn_recv;

	server(const char* ip, uint16_t port);
	~server();
	void      update();
	void      close();
	uint16_t  get_port() const;
	void      event_recv(fn_recv fn);

private:
	server_impl* impl_;
};

