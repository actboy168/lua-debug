#pragma once

#include <string>
#include <debugger/io/stream.h>

namespace bee::net {
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
		void   on_close_event(CloseEvent fn, void* ud);

	protected:
		size_t raw_peek();
		bool   raw_recv(char* buf, size_t len);
		bool   raw_send(const char* buf, size_t len);

	private:
		bee::net::namedpipe* pipe;
		CloseEvent close_event_fn = nullptr;
		void*      close_event_ud = nullptr;
	};
}}
