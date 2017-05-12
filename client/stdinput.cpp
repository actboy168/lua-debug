#include "stdinput.h"

#include <thread>
#include "dbg_protocol.h"
#include "dbg_format.h"
#include <net/queue.h>


stdinput::stdinput()
	: std::thread(std::bind(&stdinput::run, this))
{ }

void stdinput::run() {
	for (;;) {
		while (update()) {}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}
bool stdinput::update() {
	char buf[32] = { 0 };
	if (fgets(buf, sizeof buf - 1, stdin) == NULL) {
		return false;
	}
	buf[sizeof buf - 1] = 0;
	if (strncmp(buf, "Content-Length: ", 16) != 0) {
		return false;
	}
	char *bufp = buf + 16;
	int len = atoi(bufp);
	if (len <= 0) {
		exit(1);
	}
	if (fgets(buf, sizeof buf, stdin) == NULL) {
		exit(1);
	}
	buffer_.resize(len + 1);
	buffer_[0] = 0;
	if (fread(buffer_.data(), len, 1, stdin) != 1) {
		exit(1);
	}
	buffer_[len] = 0;
	rapidjson::Document	d;
	if (d.Parse(buffer_.data(), len).HasParseError()) {
		exit(1);
	}
	input_queue_.push(vscode::rprotocol(std::move(d)));
	return true;
}
bool stdinput::empty() const {
	return input_queue_.empty();
}
vscode::rprotocol stdinput::pop() {
	vscode::rprotocol r = std::move(input_queue_.front());
	input_queue_.pop();
	return r;
}
void stdinput::output(const char* str, size_t len) {
	for (;;) {
		size_t r = fwrite(str, len, 1, stdout);
		if (r == 1)
			break;
	}
}
void stdinput::output(const vscode::wprotocol& wp) {
	if (!wp.IsComplete())
		return;
	auto l = vscode::format("Content-Length: %d\r\n\r\n", wp.size());
	output(l.data(), l.size());
	output(wp.data(), wp.size());
}
