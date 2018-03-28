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

std::string fileio::input() {
	if (input_.empty()) {
		return std::string();
	}
	std::string r = std::move(input_.front());
	input_.pop();
	return r;
}

bool fileio::input_empty() const {
	return input_.empty();
}

bool fileio::output(const char* buf, size_t len) {
	auto l = base::format("Content-Length: %d\r\n\r\n", len);
	for (;;) {
		size_t r = fwrite(l.data(), l.size(), 1, fout_);
		if (r == 1)
			break;
	}
	for (;;) {
		size_t r = fwrite(buf, len, 1, fout_);
		if (r == 1)
			break;
	}
	return true;
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

std::string stdinput::input() {
	if (preinput_.empty()) {
		return fileio::input();
	}
	std::string r = std::move(preinput_.front());
	preinput_.pop();
	return r;
}

bool stdinput::input_empty() const {
	return preinput_.empty() && fileio::input_empty();
}

void stdinput::push_input(std::string&& rp)
{
	preinput_.push(std::forward<std::string>(rp));
}

void stdinput::update(int ms) {
}
