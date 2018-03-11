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

inline uint16_t wait_ok(server* s) {
	uint16_t port = 0;
	for (int i = 0; i < 10; ++i) {
		port = s->get_port();
		if (port) {
			return port;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		s->update();
	}
	return 0;
}
