
#include <bee/subprocess.h>
#include <bee/utility/unicode.h>
#include <bee/utility/path_helper.h>
#include <base/hook/injectdll.h>
#include <debugger/protocol.h>
#include <debugger/client/run.h>

process_opt create_process_with_debugger(vscode::rprotocol& req, bool noinject)
{
	auto& args = req["arguments"];
	if (!args.HasMember("runtimeExecutable") || !args["runtimeExecutable"].IsString()) {
		return process_opt();
	}
	std::wstring wapplication = bee::u2w(args["runtimeExecutable"].Get<std::string>());
	std::wstring wcwd;
	if (args.HasMember("cwd") && args["cwd"].IsString()) {
		wcwd = bee::u2w(args["cwd"].Get<std::string>());
	}
	else {
		wcwd = fs::path(wapplication).parent_path();
	}

	auto dir = bee::path_helper::exe_path().value().parent_path().parent_path();
	bee::subprocess::spawn spawn;
	spawn.set_console(bee::subprocess::console::eNew);

	if (args.HasMember("env")) {
		if (args["env"].IsObject()) {
			for (auto& v : args["env"].GetObject()) {
				if (v.name.IsString()) {
					if (v.value.IsString()) {
						spawn.env_set(bee::u2w(v.name.Get<std::string>()), bee::u2w(v.value.Get<std::string>()));
					}
					else if (v.value.IsNull()) {
						spawn.env_del(bee::u2w(v.name.Get<std::string>()));
					}
				}
			}
		}
		req.RemoveMember("env");
	}

	if (args.HasMember("runtimeArgs")) {
		if (args["runtimeArgs"].IsString()) {
            bee::subprocess::args_t wargs;
            wargs.type = bee::subprocess::args_t::type::string;
            wargs.push_back(wapplication);
            wargs.push_back(bee::u2w(args["runtimeArgs"].Get<std::string>()));
            if (!spawn.exec(wargs, wcwd.c_str())) {
				return process_opt();
			}
		}
		else if (args["runtimeArgs"].IsArray()) {
			bee::subprocess::args_t wargs;
			wargs.push_back(wapplication);
			for (auto& v : args["runtimeArgs"].GetArray()) {
				if (v.IsString()) {
					wargs.push_back(bee::u2w(std::string_view(v.GetString(), v.GetStringLength())));
				}
			}
			if (!spawn.exec(wargs, wcwd.c_str())) {
				return process_opt();
			}
		}
	}
	else {
        bee::subprocess::args_t wargs;
        wargs.push_back(wapplication);
		if (!spawn.exec(wargs, wcwd.c_str())) {
			return process_opt();
		}
	}
	if (noinject) {
		return bee::subprocess::process(spawn);
	}
	spawn.suspended();
	auto process = bee::subprocess::process(spawn);
	base::hook::injectdll(process.info(), dir / L"x86" / L"debugger-inject.dll", dir / L"x64" / L"debugger-inject.dll");
	process.resume();
	return process;
}
