#include "launch.h"
#include "stdinput.h"

launch::launch(stdinput& io)
	: debugger_(&io, vscode::threadmode::sync, vscode::coding::ansi)
	, io_(io)
{
}

void launch::update()
{
	debugger_.update();
}

void launch::send(vscode::rprotocol&& rp)
{
	io_.push_input(std::forward<vscode::rprotocol>(rp));
}
