#pragma once

#include <debugger/io/base.h>
#include <net/queue.h>

namespace vscode { namespace io {
	struct stream
		: public base
	{
		virtual size_t raw_peek() = 0;
		virtual bool raw_recv(char* buf, size_t len) = 0;
		virtual bool raw_send(const char* buf, size_t len) = 0;
		virtual void close() = 0;

		stream();
		void update(int ms);
		bool output(const char* buf, size_t len);
		bool input(std::string& buf);

		net::queue<std::string, 8> queue;
		std::string                buf;
		size_t                     stat;
		size_t                     len;
	};
}}
