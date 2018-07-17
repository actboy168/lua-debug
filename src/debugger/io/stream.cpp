#include <debugger/io/stream.h>
#include <base/util/format.h>

namespace vscode { namespace io {
	stream::stream()
	: stat(0)
	, buf()
	, len(0)
	{ }

	void stream::update(int ms) {
		for (size_t n = raw_peek(); n; --n)
		{
			char c = 0;
			raw_recv(&c, 1);
			buf.push_back(c);
			switch (stat)
			{
			case 0:
				if (c == '\r') stat = 1;
				break;
			case 1:
				stat = 0;
				if (c == '\n') {
					if (buf.substr(0, 16) != "Content-Length: ") {
						close();
						return;
					}
					try {
						len = (size_t)std::stol(buf.substr(16, buf.size() - 18));
						stat = 2;
					}
					catch (...) {
						close();
						return;
					}
					buf.clear();
				}
				break;
			case 2:
				if (buf.size() >= (len + 2))
				{
					queue.enqueue(buf.substr(2, len));
					buf.clear();
					stat = 0;
				}
				break;
			}
		}
	}

	bool stream::output(const char* buf, size_t len) {
		auto l = ::base::format("Content-Length: %d\r\n\r\n", len);
		if (!raw_send(l.data(), l.size())) {
			return false;
		}
		if (!raw_send(buf, len)) {
			return false;
		}
		return true;
	}

	bool stream::input(std::string& buf) {
		return queue.try_dequeue(buf);
	}
}}
