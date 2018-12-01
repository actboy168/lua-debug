#include <debugger/io/namedpipe.h>

#if defined(_WIN32)

#include <base/util/format.h>
#include <net/namedpipe.h>

namespace vscode { namespace io {
	namedpipe::namedpipe()
	: pipe(new bee::net::namedpipe)
	{ }

	namedpipe::~namedpipe()
	{
		pipe->close();
		delete pipe;
	}

	bool namedpipe::open_server(std::wstring const& name) {
		return pipe->open_server(name);
	}

	bool namedpipe::open_client(std::wstring const& name, int timeout) {
		return pipe->open_client(name, timeout);
	}

	size_t namedpipe::raw_peek() {
		return pipe->peek();
	}
	bool namedpipe::raw_recv(char* buf, size_t len) {
		size_t rn = len;
		return pipe->recv(buf, rn) && rn == len;
	}
	bool namedpipe::raw_send(const char* buf, size_t len) {
		size_t wn = len;
		return pipe->send(buf, wn) && wn == len;
	}
	void namedpipe::close() {
		pipe->close();
		if (close_event_fn) {
			auto fn = close_event_fn;
			close_event_fn = nullptr;
			fn(close_event_ud);
		}
	}
	bool namedpipe::is_closed() const {
		return pipe->is_closed();
	}
	void namedpipe::on_close_event(CloseEvent fn, void* ud) {
		close_event_fn = fn;
		close_event_ud = ud;
	}
}}
#else

namespace vscode { namespace io {
	namedpipe::namedpipe()
	: pipe(nullptr)
	{ }

	namedpipe::~namedpipe()
	{
	}

	bool namedpipe::open_server(std::wstring const& name) {
		return false;
	}

	bool namedpipe::open_client(std::wstring const& name, int timeout) {
		return false;
	}

	size_t namedpipe::raw_peek() {
		return 0;
	}
	bool namedpipe::raw_recv(char* buf, size_t len) {
		return false;
	}
	bool namedpipe::raw_send(const char* buf, size_t len) {
		return false;
	}
	void namedpipe::close() {
	}
	bool namedpipe::is_closed() const {
		return true;
	}
	void namedpipe::on_close_event(CloseEvent fn, void* ud) {
	}
}}
#endif
