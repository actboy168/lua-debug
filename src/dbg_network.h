#pragma once

#include "dbg_io.h"

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

	class DEBUGGER_API network
		: public io
	{
	public:
		network(const char* ip, uint16_t port);
		virtual   ~network();
		void      update(int ms);
		bool      output(const wprotocol& wp);
		rprotocol input();
		bool      input_empty() const;
		void      close();
		void      set_schema(const char* file);

	private:
		net::poller_t* poller_;
		server*        server_;
	};
}
