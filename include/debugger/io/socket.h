#pragma once

#include <debugger/io/base.h>
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

namespace vscode { namespace io {
	class server;

	class DEBUGGER_API socket
		: public base
	{
	public:
		socket(const char* ip, uint16_t port, bool rebind);
		virtual   ~socket();
		void      update(int ms);
		bool      output(const char* buf, size_t len);
		bool      input(std::string& buf);
		void      close();
		uint16_t  get_port() const;

	private:
		net::poller_t* poller_;
		server*        server_;
	};
}}
