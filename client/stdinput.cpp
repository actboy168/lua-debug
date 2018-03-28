#include "stdinput.h"
#include <base/util/format.h>

fileio::fileio(FILE* fin, FILE* fout)
	: fin_(fin)
	, fout_(fout)
{ }

void fileio::update(int ms) {
	for (;;) {
		char buf[32] = { 0 };
		if (fgets(buf, sizeof buf - 1, fin_) == NULL) {
			break;
		}
		buf[sizeof buf - 1] = 0;
		if (strncmp(buf, "Content-Length: ", 16) != 0) {
			break;
		}
		char *bufp = buf + 16;
		int len = atoi(bufp);
		if (len <= 0) {
			close();
		}
		if (fgets(buf, sizeof buf, fin_) == NULL) {
			close();
		}
		buffer_.resize(len + 1);
		buffer_[0] = 0;
		if (fread(buffer_.data(), len, 1, fin_) != 1) {
			close();
		}
		buffer_[len] = 0;
		input_.push(std::string(buffer_.data(), len));
	}
}

bool fileio::input(std::string& buf) {
	if (input_.empty()) {
		return false;
	}
	buf = std::move(input_.front());
	input_.pop();
	return true;
}

bool fileio::output(const char* buf, size_t len) {
	auto l = base::format("Content-Length: %d\r\n\r\n", len);
	raw_output(l.data(), l.size());
	raw_output(buf, len);
	return true;
}

void fileio::raw_output(const char* buf, size_t len) {
	for (;;) {
		size_t r = fwrite(buf, len, 1, fout_);
		if (r == 1)
			break;
	}
}

stdinput::stdinput()
	: fileio(stdin, stdout)
	, std::thread(std::bind(&stdinput::run, this))
{ }

void stdinput::run() {
	for (;;) {
		fileio::update(10);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void stdinput::close() {
	exit(0);
}

bool stdinput::input(std::string& buf) {
	if (preinput_.empty()) {
		return fileio::input(buf);
	}
	buf = std::move(preinput_.front());
	preinput_.pop();
	return true;
}

void stdinput::push_input(std::string&& rp)
{
	preinput_.push(std::forward<std::string>(rp));
}

void stdinput::update(int ms) {
}
