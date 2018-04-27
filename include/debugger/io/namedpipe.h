#pragma once

#include <string>
#include <debugger/io/stream.h>
#include <net/queue.h>

namespace net {
	class namedpipe;
}

namespace vscode { namespace io {
	class namedpipe
		: public stream
	{
	public:
		namedpipe();
		~namedpipe();
		bool   open_server(std::wstring const& name);
		bool   open_client(std::wstring const& name, int timeout);
		void   close();
		bool   is_closed() const;
		void   kill_when_close();

	protected:
		size_t raw_peek();
		bool   raw_recv(char* buf, size_t len);
		bool   raw_send(const char* buf, size_t len);

	private:
		net::namedpipe* pipe;
		bool kill_when_close_ = false;
	};
}}
