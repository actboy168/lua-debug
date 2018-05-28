#pragma once

#include <debugger/io/base.h>
#include <net/queue.h>

namespace vscode { namespace io {
	struct DEBUGGER_API stream
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
#if defined(_WIN32)
#pragma warning(push)
#pragma warning(disable:4251)
#endif
		net::queue<std::string, 8> queue;
		std::string                buf;
#if defined(_WIN32)
#pragma warning(pop)
#endif
		size_t                     stat;
		size_t                     len;
	};
}}
