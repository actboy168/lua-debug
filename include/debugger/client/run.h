#pragma once

#include <debugger/protocol.h>

class stdinput;

int run_launch(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req);
bool run_pipe_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req, const std::wstring& pipename);

bool create_process_with_debugger(vscode::rprotocol& req, const std::wstring& port);
bool create_terminal_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port);
bool create_luaexe_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port);
