#include "launch.h"
#include "dbg_format.h"
#include "dbg_unicode.h"
#include "stdinput.h"
#include <Windows.h>

launch_server::launch_server(stdinput& io)
	: debugger_(&io, vscode::threadmode::sync)
	, io_(io)
{
}

void launch_server::update()
{
	debugger_.update();
}

void launch_server::send(vscode::rprotocol&& rp)
{
	io_.push_input(std::forward<vscode::rprotocol>(rp));
}
