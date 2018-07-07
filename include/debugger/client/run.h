#pragma once

#include <debugger/protocol.h>
#include <base/filesystem.h>

namespace base { namespace win {
	struct process_switch;
}}

class stdinput;

std::string cmd_string(const std::string& str);
std::wstring cmd_string(const std::wstring& str);
std::string create_install_script(vscode::rprotocol& req, const fs::path& dbg_path, const std::wstring& port, bool redirect);
int getLuaRuntime(const rapidjson::Value& args);
bool is64Exe(const wchar_t* exe);

bool run_pipe_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req, const std::wstring& pipename, base::win::process_switch* m = nullptr);
bool open_process_with_debugger(vscode::rprotocol& req, int pid);
bool create_process_with_debugger(vscode::rprotocol& req, int& pid);
bool create_terminal_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port);
bool create_luaexe_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port);
