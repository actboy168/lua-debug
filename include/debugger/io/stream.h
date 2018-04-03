#pragma once

#include <debugger/io/base.h>
#include <net/queue.h>

#if defined(DEBUGGER_INLINE)
#	define DEBUGGER_API
#else
#	if defined(DEBUGGER_EXPORTS)
#		define DEBUGGER_API __declspec(dllexport)
#	else
#		define DEBUGGER_API __declspec(dllimport)
#	endif
#endif

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

#pragma warning(push)
#pragma warning(disable:4251)
		net::queue<std::string, 8> queue;
		std::string                buf;
#pragma warning(pop)
		size_t                     stat;
		size_t                     len;
	};
}}
