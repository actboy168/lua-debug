#include "stdinput.h"
#include "dbg_format.h"

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
		rapidjson::Document	d;
		if (d.Parse(buffer_.data(), len).HasParseError()) {
			exit(1);
		}
		input_queue_.push(vscode::rprotocol(std::move(d)));
	}
}

vscode::rprotocol fileio::input() {
	vscode::rprotocol r = std::move(input_queue_.front());
	input_queue_.pop();
	return r;
}

bool fileio::input_empty() const {
	return input_queue_.empty();
}

bool fileio::output(const vscode::wprotocol& wp) {
	if (!wp.IsComplete())
		return false;
	auto l = vscode::format("Content-Length: %d\r\n\r\n", wp.size());
	output(l.data(), l.size());
	output(wp.data(), wp.size());
	return true;
}

void fileio::output(const char* str, size_t len) {
	for (;;) {
		size_t r = fwrite(str, len, 1, fout_);
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