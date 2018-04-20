#include <Windows.h>
#include <debugger/client/run.h>
#include <debugger/client/stdinput.h>
#include <base/util/format.h>
#include <base/path/self.h>
#include <base/win/process.h>

std::string create_install_script(vscode::rprotocol& req, const fs::path& dbg_path, const std::wstring& port)
{
	auto& args = req["arguments"];
	bool isUtf8 = false;
	std::string sourceCoding = "ansi";
	if (args.HasMember("sourceCoding") && args["sourceCoding"].IsString()) {
		isUtf8 = "utf8" == args["sourceCoding"].Get<std::string>();
	}
	std::string res;
	if (args.HasMember("path") && args["path"].IsString()) {
		res += base::format("package.path=[[%s]];", isUtf8 ? args["path"].Get<std::string>() : base::u2a(args["path"]));
	}
	if (args.HasMember("cpath") && args["cpath"].IsString()) {
		res += base::format("package.cpath=[[%s]];", isUtf8 ? args["cpath"].Get<std::string>() : base::u2a(args["cpath"]));
	}
	res += base::format("local dbg=package.loadlib([[%s]], 'luaopen_debugger')();package.loaded['debugger']=dbg;dbg:listen([[pipe:%s]]):redirect('print'):redirect('stdout'):redirect('stderr'):guard():start()"
		, isUtf8 ? base::w2u((dbg_path / L"debugger.dll").wstring()) : base::w2a((dbg_path / L"debugger.dll").wstring())
		, base::w2u(port)
	);
	return res;
}

bool create_luaexe_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port)
{
	auto& args = req["arguments"];
	base::win::process p;
	fs::path dbg_path = base::path::self().remove_filename();
	std::wstring luaexe;
	if (args.HasMember("luaexe") && args["luaexe"].IsString()) {
		luaexe = base::u2w(args["luaexe"].Get<std::string>());
	}
	else {
		luaexe = dbg_path / "lua.exe";
		if (args.HasMember("luadll") && args["luadll"].IsString()) {
			p.replace(base::u2w(args["luadll"].Get<std::string>()), "lua53.dll");
		}
	}

	std::wstring wcommand = L"\"" + luaexe + L"\"";
	std::wstring wcwd;
	if (args.HasMember("cwd") && args["cwd"].IsString()) {
		wcwd = base::u2w(args["cwd"].Get<std::string>());
	}
	else {
		wcwd = fs::path(luaexe).remove_filename();
	}
	std::string script = create_install_script(req, dbg_path, port);

	wcommand += base::format(LR"( -e "%s")", base::u2w(script));
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
	return p.create(luaexe.c_str(), wcommand.c_str(), wcwd.c_str());
}
