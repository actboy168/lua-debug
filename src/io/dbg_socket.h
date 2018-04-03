#pragma once

#include "dbg_io.h"
#include <stdint.h>

#if defined(DEBUGGER_INLINE)
#	define DEBUGGER_API
#else
#	if defined(DEBUGGER_EXPORTS)
#		define DEBUGGER_API __declspec(dllexport)
#	else
#		define DEBUGGER_API __declspec(dllimport)
#	endif
#endif

namespace net {
	struct poller_t;
}

namespace vscode
{
	class server;

	class DEBUGGER_API io_socket
		: public io
	{
	public:
		io_socket(const char* ip, uint16_t port, bool rebind);
		virtual   ~io_socket();
		void      update(int ms);
		bool      output(const char* buf, size_t len);
		bool      input(std::string& buf);
		void      close();
		void      kill_process_when_close();
		uint16_t  get_port() const;

	private:
		net::poller_t* poller_;
		server*        server_;
		bool           kill_process_when_close_;
	};
}
