#pragma once

#include <string>
#include <memory>
#include <net/poller.h>
#include "dbg_message.h"

namespace vscode
{
	class server;
	class rprotocol;
	class wprotocol;

	class network
	{
	public:
		network(const char* ip, uint16_t port);
		void      update(int ms);
		bool      output(const wprotocol& wp);
		rprotocol input();
		bool      input_empty() const;
		void      set_schema(const char* file);
		void      close_session();

	private:
		std::unique_ptr<net::poller_t> poller_;
		std::unique_ptr<server> server_;
	};
}
