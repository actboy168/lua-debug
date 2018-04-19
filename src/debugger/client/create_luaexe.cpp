#include <Windows.h>
#include <debugger/client/run.h>
#include <debugger/client/stdinput.h>
#include <base/util/format.h>
#include <base/path/self.h>
#include <base/win/process.h>

bool create_luaexe_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port)
{
	auto& args = req["arguments"];
	std::wstring wapplication = base::path::self().remove_filename() / "lua.exe";
	std::wstring wcommand = L"\"" + wapplication + L"\"";
	std::wstring wcwd;
	if (args.HasMember("cwd") && args["cwd"].IsString()) {
		wcwd = base::u2w(args["cwd"].Get<std::string>());
	}
	else {
		wcwd = fs::path(wapplication).remove_filename();
	}

	std::wstring preenv = base::format(L"require([[debugger]]):listen([[pipe:%s]]):redirect('print'):redirect('stdout'):redirect('stderr'):start()", port);
	if (args.HasMember("path") && args["path"].IsString()) {
		preenv += base::format(L";package.path=[[%s]]", base::u2w(args["path"]));
	}
	if (args.HasMember("cpath") && args["cpath"].IsString()) {
		preenv += base::format(L";package.cpath=[[%s]]", base::u2w(args["cpath"]));
	}

	wcommand += base::format(LR"( -e "%s")", preenv);
	if (args.HasMember("arg0")) {
		if (args["arg0"].IsString()) {
			auto& v = args["arg0"];
			wcommand += base::format(LR"( "%s")", base::u2w(v));
		}
		else if (args["arg0"].IsArray()) {
			for (auto& v : args["arg0"].GetArray()) {
				if (v.IsString()) {
					wcommand += base::format(LR"( "%s")", base::u2w(v));
				}
			}
		}
	}

	std::wstring program = L".lua";
	if (args.HasMember("program") && args["program"].IsString()) {
		program = base::u2w(args["program"]);
	}
	wcommand += base::format(LR"( "%s")", program);

	if (args.HasMember("arg") && args["arg"].IsArray()) {
		for (auto& v : args["arg"].GetArray()) {
			if (v.IsString()) {
				wcommand += base::format(LR"( "%s")", base::u2w(v));
			}
		}
	}

	base::win::process p;

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
	return p.create(wapplication.c_str(), wcommand.c_str(), wcwd.c_str());
}
