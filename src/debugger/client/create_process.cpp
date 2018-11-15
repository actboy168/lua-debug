
#include <base/subprocess.h>
#include <base/util/unicode.h>
#include <base/path/self.h>
#include <base/hook/injectdll.h>
#include <debugger/protocol.h>
#include <debugger/client/run.h>

process_opt create_process_with_debugger(vscode::rprotocol& req)
{
	auto& args = req["arguments"];
	if (!args.HasMember("runtimeExecutable") || !args["runtimeExecutable"].IsString()) {
		return process_opt();
	}
	std::wstring wapplication = base::u2w(args["runtimeExecutable"].Get<std::string>());
	std::wstring wcwd;
	if (args.HasMember("cwd") && args["cwd"].IsString()) {
		wcwd = base::u2w(args["cwd"].Get<std::string>());
	}
	else {
		wcwd = fs::path(wapplication).parent_path();
	}

	auto dir = base::path::self().parent_path().parent_path();
	base::subprocess::spawn spawn;
	spawn.set_console(base::subprocess::console::eNew);
	spawn.suspended();

	if (args.HasMember("env")) {
		if (args["env"].IsObject()) {
			for (auto& v : args["env"].GetObject()) {
				if (v.name.IsString()) {
					if (v.value.IsString()) {
						spawn.env_set(base::u2w(v.name.Get<std::string>()), base::u2w(v.value.Get<std::string>()));
					}
					else if (v.value.IsNull()) {
						spawn.env_del(base::u2w(v.name.Get<std::string>()));
					}
				}
			}
		}
		req.RemoveMember("env");
	}

	if (args.HasMember("runtimeArgs")) {
		if (args["runtimeArgs"].IsString()) {
			if (!spawn.exec(wapplication, base::u2w(args["runtimeArgs"].Get<std::string>()), wcwd.c_str())) {
				return process_opt();
			}
		}
		else if (args["runtimeArgs"].IsArray()) {
			std::vector<std::wstring> wargs;
			wargs.push_back(wapplication);
			for (auto& v : args["runtimeArgs"].GetArray()) {
				if (v.IsString()) {
					wargs.push_back(base::u2w(v));
				}
			}
			if (!spawn.exec(wargs, wcwd.c_str())) {
				return process_opt();
			}
		}
	}
	else {
		if (!spawn.exec({ wapplication }, wcwd.c_str())) {
			return process_opt();
		}
	}

	auto process = base::subprocess::process(spawn);
	base::hook::injectdll(process, dir / L"x86" / L"debugger-inject.dll", dir / L"x64" / L"debugger-inject.dll");
	process.resume();
	return process;
}
