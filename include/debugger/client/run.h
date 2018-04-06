#pragma once

#include <debugger/protocol.h>

class stdinput;

int run_launch(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req);
int run_terminal_then_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req);
