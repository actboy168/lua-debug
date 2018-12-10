#include <debugger/client/stdinput.h>
#include <bee/utility/format.h>
#include <functional>

#if 1
#	define log(...)
#else

#include <bee/utility/format.h>
#include <Windows.h>

template <class... Args>
static void log(const char* fmt, const Args& ... args)
{
    auto s = bee::format(fmt, args...);
    OutputDebugStringA(s.c_str());
}

#endif

stdinput::stdinput()
    : input_()
    , preinput_()
    , buffer_()
    , thread_(std::bind(&stdinput::run, this))
{ }

stdinput::~stdinput() {
    fflush(stdout);
    fflush(stderr);
    thread_.detach();
    close();
}

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
		input_.enqueue(std::string(buffer_.data(), len));
	}
}

bool stdinput::output(const char* buf, size_t len) {
	auto l = bee::format("Content-Length: %d\r\n\r\n", len);
	raw_output(l.data(), l.size());
	raw_output(buf, len);
	return true;
}

void stdinput::raw_output(const char* buf, size_t len) {
	for (;;) {
        log("%s", std::string(buf, len));
		size_t r = fwrite(buf, len, 1, stdout);
		if (r == 1)
			break;
	}
}

void stdinput::close() {
	exit(0);
}

bool stdinput::input(std::string& buf) {
	if (preinput_.try_dequeue(buf)) {
		return true;
	}
	if (input_.try_dequeue(buf)) {
		return true;
	}
	return false;
}

void stdinput::push_input(vscode::rprotocol& rp)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	rp.Accept(writer);
	preinput_.enqueue(std::string(buffer.GetString(), buffer.GetSize()));
}

void stdinput::update(int ms) {
}
