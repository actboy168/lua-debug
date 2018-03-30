#include "stdinput.h"
#include <base/util/format.h>

stdinput::stdinput()
	: std::thread(std::bind(&stdinput::run, this))
{ }

void stdinput::run() {
	for (;;) {
		char buf[32] = { 0 };
		if (fgets(buf, sizeof buf - 1, stdin) == NULL) {
			close();
		}
		buf[sizeof buf - 1] = 0;
		if (strncmp(buf, "Content-Length: ", 16) != 0) {
			close();
		}
		char *bufp = buf + 16;
		int len = atoi(bufp);
		if (len <= 0) {
			close();
		}
		if (fgets(buf, sizeof buf, stdin) == NULL) {
			close();
		}
		buffer_.resize(len + 1);
		buffer_[0] = 0;
		if (fread(buffer_.data(), len, 1, stdin) != 1) {
			close();
		}
		buffer_[len] = 0;
		input_.push(std::string(buffer_.data(), len));
	}
}

bool stdinput::output(const char* buf, size_t len) {
	auto l = base::format("Content-Length: %d\r\n\r\n", len);
	raw_output(l.data(), l.size());
	raw_output(buf, len);
	return true;
}

void stdinput::raw_output(const char* buf, size_t len) {
	for (;;) {
		size_t r = fwrite(buf, len, 1, stdout);
		if (r == 1)
			break;
	}
}

void stdinput::close() {
	exit(0);
}

bool stdinput::input(std::string& buf) {
	if (!preinput_.empty()) {
		buf = std::move(preinput_.front());
		preinput_.pop();
		return true;
	}
	if (!input_.empty()) {
		buf = std::move(input_.front());
		input_.pop();
		return true;
	}
	return false;
}

void stdinput::push_input(const char* buf, size_t len)
{
	preinput_.push(std::string(buf, len));
}

void stdinput::update(int ms) {
}
