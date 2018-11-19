#pragma once

#include <debugger/protocol.h>
#include <base/filesystem.h>
#include <base/subprocess.h>
#include <optional>

namespace base { namespace win {
	struct process_switch;
}}

class stdinput;

typedef std::optional<base::subprocess::process> process_opt;

std::string create_install_script(vscode::rprotocol& req, const fs::path& dbg_path, const std::wstring& port);
int getLuaRuntime(const rapidjson::Value& args);
bool is64Exe(const wchar_t* exe);

base::subprocess::process openprocess(int pid);

bool run_pipe_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req, const std::wstring& pipename, process_opt& p, base::win::process_switch* m = nullptr);
bool open_process_with_debugger(vscode::rprotocol& req, int pid);
process_opt create_process_with_debugger(vscode::rprotocol& req, bool noinject);
bool create_terminal_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port);
process_opt create_luaexe_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port);
