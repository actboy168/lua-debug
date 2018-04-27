#include <debugger/io/namedpipe.h>
#include <base/util/format.h>
#include <net/namedpipe.h>

namespace vscode { namespace io {
	namedpipe::namedpipe()
	: pipe(new net::namedpipe)
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
		if (kill_when_close_) {
			exit(0);
		}
	}
	bool namedpipe::is_closed() const {
		return pipe->is_closed();
	}
	void namedpipe::kill_when_close() {
		kill_when_close_ = true;
	}
}}
