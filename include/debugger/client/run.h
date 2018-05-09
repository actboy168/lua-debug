#pragma once

#include <debugger/protocol.h>
#include <base/filesystem.h>

class stdinput;

bool run_pipe_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req, const std::wstring& pipename);

std::string create_install_script(vscode::rprotocol& req, const fs::path& dbg_path, const std::wstring& port, bool redirect);

bool create_process_with_debugger(vscode::rprotocol& req, const std::wstring& port);
bool create_terminal_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port);
bool create_luaexe_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port);
