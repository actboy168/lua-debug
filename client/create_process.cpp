
#include <base/win/process.h>
#include "inject.h"
#include "dbg_protocol.h"
#include "dbg_unicode.h"

uint16_t create_process_with_debugger(vscode::rprotocol& req)
{
	auto& args = req["arguments"];
	if (args.HasMember("luadll") && args["luadll"].IsString()) {
		std::wstring wluadll = vscode::u2w(args["luadll"].Get<std::string>());
		if (!set_luadll(wluadll.data(), wluadll.size())) {
			return 0;
		}
	}

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
	if (!p.create(wapplication.c_str(), wcommand.c_str(), wcwd.c_str())) {
		return 0;
	}
	return inject_wait(p);
}
