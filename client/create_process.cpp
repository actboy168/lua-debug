
#include <base/win/process.h>
#include "inject.h"
#include "dbg_protocol.h"
#include "dbg_unicode.h"

bool create_process_with_debugger(vscode::rprotocol& req, uint16_t port)
{
	auto& args = req["arguments"];
	if (!args.HasMember("runtimeExecutable") || !args["runtimeExecutable"].IsString()) {
		return 0;
	}
	std::wstring wapplication = vscode::u2w(args["runtimeExecutable"].Get<std::string>());
	std::wstring wcommand = L"\"" + wapplication + L"\"";
	if (args.HasMember("runtimeArgs") && args["runtimeArgs"].IsString()) {
		wcommand = wcommand + L" " + vscode::u2w(args["runtimeArgs"].Get<std::string>());
	}
	std::wstring wcwd;
	if (args.HasMember("cwd") && args["cwd"].IsString()) {
		wcwd = vscode::u2w(args["cwd"].Get<std::string>());
	}
	else {
		wcwd = fs::path(wapplication).remove_filename();
	}

	base::win::process p;
	inject_start(p);

	if (args.HasMember("env")) {
		if (args["env"].IsObject()) {
			for (auto& v : args["env"].GetObject()) {
				if (v.name.IsString()) {
					if (v.value.IsString()) {
						p.set_env(vscode::u2w(v.name.Get<std::string>()), vscode::u2w(v.value.Get<std::string>()));
					}
					else if (v.value.IsNull()) {
						p.del_env(vscode::u2w(v.name.Get<std::string>()));
					}
				}
			}
		}
		req.RemoveMember("env");
	}
	p.set_env(L"LUADBG_PORT", std::to_wstring(port));
	return p.create(wapplication.c_str(), wcommand.c_str(), wcwd.c_str());
}
