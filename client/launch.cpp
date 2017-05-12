#include "launch.h"
#include "dbg_hybridarray.h"
#include "dbg_format.h"
#include "dbg_unicode.h"
#include "stdinput.h"
#include <Windows.h>

launch_io::launch_io()
	: vscode::io()
	, buffer_()
	, input_queue_()
{ }

void launch_io::update(int ms)
{
}

void launch_io::output_(const char* str, size_t len) {
	for (;;) {
		size_t r = fwrite(str, len, 1, stdout);
		if (r == 1)
			break;
	}
}

bool launch_io::output(const vscode::wprotocol& wp)
{
	if (!wp.IsComplete())
		return false;
	auto l = vscode::format("Content-Length: %d\r\n\r\n", wp.size());
	output_(l.data(), l.size());
	output_(wp.data(), wp.size());
	return true;
}

vscode::rprotocol launch_io::input()
{
	if (input_queue_.empty())
		return vscode::rprotocol();
	vscode::rprotocol r = std::move(input_queue_.front());
	input_queue_.pop();
	return r;
}

bool launch_io::input_empty() const
{
	return input_queue_.empty();
}

void launch_io::send(vscode::rprotocol&& rp)
{
	input_queue_.push(std::forward<vscode::rprotocol>(rp));
}

void launch_io::close()
{
	exit(0);
}

launch_server::launch_server(stdinput& io_)
	: launch_io_()
	, debugger_(&launch_io_, vscode::threadmode::sync)
	, io(io_)
{
	debugger_.set_custom(this);
}

void launch_server::set_state(vscode::state state)
{
}

void launch_server::update_stop()
{
	while (!io.input_empty()) {
		vscode::rprotocol rp = io.input();
		send(std::move(rp));
	}
}

void launch_server::update()
{
	debugger_.update();
}

void launch_server::send(vscode::rprotocol&& rp)
{
	launch_io_.send(std::forward<vscode::rprotocol>(rp));
}
