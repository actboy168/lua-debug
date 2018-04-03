
#include <base/win/process.h>
#include <base/util/unicode.h>
#include <base/path/self.h>
#include <debugger/protocol.h>

bool create_process_with_debugger(vscode::rprotocol& req, const std::wstring& port)
{
	auto& args = req["arguments"];
	if (!args.HasMember("runtimeExecutable") || !args["runtimeExecutable"].IsString()) {
		return 0;
	}
	std::wstring wapplication = base::u2w(args["runtimeExecutable"].Get<std::string>());
	std::wstring wcommand = L"\"" + wapplication + L"\"";
	if (args.HasMember("runtimeArgs") && args["runtimeArgs"].IsString()) {
		wcommand = wcommand + L" " + base::u2w(args["runtimeArgs"].Get<std::string>());
	}
	std::wstring wcwd;
	if (args.HasMember("cwd") && args["cwd"].IsString()) {
		wcwd = base::u2w(args["cwd"].Get<std::string>());
	}
	else {
		wcwd = fs::path(wapplication).remove_filename();
	}

	base::win::process p;
	auto dir = base::path::self().remove_filename().remove_filename();
	p.inject_x86(dir / L"x86" / L"debugger-inject.dll");
	p.inject_x64(dir / L"x64" / L"debugger-inject.dll");

	if (args.HasMember("env")) {
		if (args["env"].IsObject()) {
			for (auto& v : args["env"].GetObject()) {
				if (v.name.IsString()) {
					if (v.value.IsString()) {
						p.set_env(base::u2w(v.name.Get<std::string>()), base::u2w(v.value.Get<std::string>()));
					}
					else if (v.value.IsNull()) {
						p.del_env(base::u2w(v.name.Get<std::string>()));
					}
				}
			}
		}
		req.RemoveMember("env");
	}
	p.set_env(L"LUADBG_PORT", port);
	return p.create(wapplication.c_str(), wcommand.c_str(), wcwd.c_str());
}
