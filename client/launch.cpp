#include "launch.h"
#include "dbg_format.h"
#include "dbg_unicode.h"
#include "stdinput.h"
#include <Windows.h>

launch_io::launch_io(stdinput& io)
	: vscode::io()
	, io_(io)
	, input_queue_()
{ }

void launch_io::update(int ms)
{
	while (!io_.input_empty()) {
		vscode::rprotocol rp = io_.input();
		send(std::move(rp));
	}
}

bool launch_io::output(const vscode::wprotocol& wp)
{
	return io_.output(wp);
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
	return io_.close();
}

launch_server::launch_server(stdinput& io_)
	: launch_io_(io_)
	, debugger_(&launch_io_, vscode::threadmode::sync)
{
}

void launch_server::update()
{
	debugger_.update();
}

void launch_server::send(vscode::rprotocol&& rp)
{
	launch_io_.send(std::forward<vscode::rprotocol>(rp));
}
