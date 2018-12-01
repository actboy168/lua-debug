#include <debugger/io/namedpipe.h>
#include <Windows.h>
#include <debugger/client/run.h>
#include <debugger/io/base.h>
#include <debugger/io/helper.h>
#include <debugger/client/stdinput.h>
#include <bee/utility/format.h>
#include <base/path/self.h>
#include <functional>

void request_runInTerminal(vscode::io::base* io, std::function<void(vscode::wprotocol&)> args)
{
	vscode::wprotocol res;
	for (auto _ : res.Object())
	{
		res("type").String("request");
		//res("seq").Int64(seq++);
		res("command").String("runInTerminal");
		for (auto _ : res("arguments").Object())
		{
			args(res);
		}
	}
	vscode::io_output(io, res);
}

bool create_terminal_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port)
{
	auto& args = req["arguments"];

	request_runInTerminal(&io, [&](vscode::wprotocol& res) {
		fs::path dbgPath = base::path::self().parent_path().parent_path();
		std::string luaexe;
		if (args.HasMember("luaexe") && args["luaexe"].IsString()) {
			luaexe = args["luaexe"].Get<std::string>();
			if (is64Exe(bee::u2w(luaexe).c_str())) {
				dbgPath /= "x64";
			}
			else {
				dbgPath /= "x86";
			}
		}
		else {
			if (54064 == getLuaRuntime(args)) {
				dbgPath /= "x64";
				luaexe = bee::w2u((dbgPath / L"lua54.exe").wstring());
			}
			else if (54032 == getLuaRuntime(args)) {
				dbgPath /= "x86";
				luaexe = bee::w2u((dbgPath / L"lua54.exe").wstring());
			}
			else if (53064 == getLuaRuntime(args)) {
				dbgPath /= "x64";
				luaexe = bee::w2u((dbgPath / L"lua53.exe").wstring());
			}
			else {
				dbgPath /= "x86";
				luaexe = bee::w2u((dbgPath / L"lua53.exe").wstring());
			}
		}

		res("kind").String(args["console"] == "integratedTerminal" ? "integrated" : "external");
		res("title").String("Lua Debug");
		if (args.HasMember("cwd") && args["cwd"].IsString()) {
			res("cwd").String(args["cwd"]);
		}
		else {
			res("cwd").String(bee::w2u(fs::path(luaexe).parent_path().wstring()));
		}
		if (args.HasMember("env") && args["env"].IsObject()) {
			for (auto _ : res("env").Object()) {
				for (auto& v : args["env"].GetObject()) {
					if (v.name.IsString()) {
						if (v.value.IsString()) {
							res(v.name).String(v.value);
						}
						else if (v.value.IsNull()) {
							res(v.name).Null();
						}
					}
				}
			}
		}

		for (auto _ : res("args").Array()) {
			res.String(luaexe);

			std::string script = create_install_script(req, dbgPath, port);
			res.String("-e");
			res.String(script);

			if (args.HasMember("arg0")) {
				if (args["arg0"].IsString()) {
					auto& v = args["arg0"];
					res.String(v);
				}
				else if (args["arg0"].IsArray()) {
					for (auto& v : args["arg0"].GetArray()) {
						if (v.IsString()) {
							res.String(v);
						}
					}
				}
			}

			std::string program = ".lua";
			if (args.HasMember("program") && args["program"].IsString()) {
				program = args["program"].Get<std::string>();
			}
			res.String(program);

			if (args.HasMember("arg") && args["arg"].IsArray()) {
				for (auto& v : args["arg"].GetArray()) {
					if (v.IsString()) {
						res.String(v);
					}
				}
			}
		}
	});
	return true;
}
