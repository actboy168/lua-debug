#include <Windows.h>
#include <debugger/client/run.h>
#include <debugger/client/stdinput.h>
#include <bee/utility/format.h>
#include <base/path/self.h>
#include <base/hook/replacedll.h>

std::string create_install_script(vscode::rprotocol& req, const fs::path& dbg_path, const std::wstring& port)
{
	auto& args = req["arguments"];
	bool isUtf8 = false;
	std::string sourceCoding = "ansi";
	if (args.HasMember("sourceCoding") && args["sourceCoding"].IsString()) {
		isUtf8 = "utf8" == args["sourceCoding"].Get<std::string>();
	}
	std::string res;
	if (args.HasMember("path")) {
		if (args["path"].IsString()) {
			res += bee::format("package.path=[[%s]];", isUtf8 ? args["path"].Get<std::string>() : bee::u2a(std::string_view(args["path"].GetString(), args["path"].GetStringLength())));
		}
		else if (args["path"].IsArray()) {
			std::string path;
			for (auto& v : args["path"].GetArray()) {
				if (v.IsString()) {
					if (!path.empty()) path += ";";
					path += isUtf8 ? v.Get<std::string>() : bee::u2a(std::string_view(v.GetString(), v.GetStringLength()));
				}
			}
			res += bee::format("package.path=[[%s]];", path);
		}
	}
	if (args.HasMember("cpath")) {
		if (args["cpath"].IsString()) {
			res += bee::format("package.cpath=[[%s]];", isUtf8 ? args["cpath"].Get<std::string>() : bee::u2a(std::string_view(args["cpath"].GetString(), args["cpath"].GetStringLength())));
		}
		else if (args["cpath"].IsArray()) {
			std::string path;
			for (auto& v : args["cpath"].GetArray()) {
				if (v.IsString()) {
					if (!path.empty()) path += ";";
					path += isUtf8 ? v.Get<std::string>() : bee::u2a(std::string_view(v.GetString(), v.GetStringLength()));
				}
			}
			res += bee::format("package.cpath=[[%s]];", path);
		}
	}
	res += bee::format("local dbg=package.loadlib([[%s]], 'luaopen_debugger')();package.loaded[ [[%s]] ]=dbg;dbg:io([[pipe:%s]])"
		, isUtf8 ? bee::w2u((dbg_path / L"debugger.dll").wstring()) : bee::w2a((dbg_path / L"debugger.dll").wstring())
		, args.HasMember("internalModule") && args["internalModule"].IsString() ? args["internalModule"].Get<std::string>() : "debugger"
		, bee::w2u(port)
	);
	if (args.HasMember("outputCapture") && args["outputCapture"].IsArray()) {
		for (auto& v : args["outputCapture"].GetArray()) {
			if (v.IsString()) {
				std::string item = v.Get<std::string>();
				if (item == "print" || item == "stdout" || item == "stderr") {
					res += ":redirect('" + item + "')";
				}
			}
		}
	}
	res += ":guard():wait():start()";
	return res;
}

int getLuaRuntime(const rapidjson::Value& args) {
	if (args.HasMember("luaRuntime") && args["luaRuntime"].IsString()) {
		std::string luaRuntime = args["luaRuntime"].Get<std::string>();
		if (luaRuntime == "5.4 64bit") {
			return 54064;
		}
		else if (luaRuntime == "5.4 32bit") {
			return 54032;
		}
		else if (luaRuntime == "5.3 64bit") {
			return 53064;
		}
		else if (luaRuntime == "5.3 32bit") {
			return 53032;
		}
	}
	return 53032;
}

bool is64Exe(const wchar_t* exe)
{
	HANDLE hExe = CreateFileW(exe, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hExe == INVALID_HANDLE_VALUE) {
		return false;
	}
	DWORD read;
	char data[sizeof IMAGE_NT_HEADERS + sizeof IMAGE_DOS_HEADER];
	SetFilePointer(hExe, 0, NULL, FILE_BEGIN);
	if (!ReadFile(hExe, data, sizeof IMAGE_DOS_HEADER, &read, NULL)) {
		CloseHandle(hExe);
		return false;
	}
	SetFilePointer(hExe, ((PIMAGE_DOS_HEADER)data)->e_lfanew, NULL, FILE_BEGIN);
	if (!ReadFile(hExe, data, sizeof IMAGE_NT_HEADERS, &read, NULL)) {
		CloseHandle(hExe);
		return false;
	}
	CloseHandle(hExe);
	return !(((PIMAGE_NT_HEADERS)data)->FileHeader.Characteristics & IMAGE_FILE_32BIT_MACHINE);
}

process_opt create_luaexe_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port)
{
	auto& args = req["arguments"];
	fs::path dbgPath = base::path::self().parent_path().parent_path();
	std::wstring luaexe;
	std::pair<std::string, std::string> replacedll;
	if (args.HasMember("luaexe") && args["luaexe"].IsString()) {
		luaexe = bee::u2w(args["luaexe"].Get<std::string>());
		if (is64Exe(luaexe.c_str())) {
			dbgPath /= "x64";
		}
		else {
			dbgPath /= "x86";
		}
	}
	else {
		if (54064 == getLuaRuntime(args)) {
			dbgPath /= "x64";
			luaexe = dbgPath / "lua54.exe";
			if (args.HasMember("luadll") && args["luadll"].IsString()) {
				replacedll = { bee::u2a(args["luadll"].Get<std::string>()), "lua54.dll" };
			}
		}
		else if (54032 == getLuaRuntime(args)) {
			dbgPath /= "x86";
			luaexe = dbgPath / "lua54.exe";
			if (args.HasMember("luadll") && args["luadll"].IsString()) {
				replacedll = { bee::u2a(args["luadll"].Get<std::string>()), "lua54.dll" };
			}
		}
		else if (53064 == getLuaRuntime(args)) {
			dbgPath /= "x64";
			luaexe = dbgPath / "lua53.exe";
			if (args.HasMember("luadll") && args["luadll"].IsString()) {
				replacedll = { bee::u2a(args["luadll"].Get<std::string>()), "lua53.dll" };
			}
		}
		else {
			dbgPath /= "x86";
			luaexe = dbgPath / "lua53.exe";
			if (args.HasMember("luadll") && args["luadll"].IsString()) {
				replacedll = { bee::u2a(args["luadll"].Get<std::string>()), "lua53.dll" };
			}
		}
	}

	std::wstring wcwd;
	std::vector<std::wstring> wargs;
	wargs.push_back(luaexe);

	if (args.HasMember("cwd") && args["cwd"].IsString()) {
		wcwd = bee::u2w(args["cwd"].Get<std::string>());
	}
	else {
		wcwd = fs::path(luaexe).parent_path();
	}
	std::string script = create_install_script(req, dbgPath, port);

	wargs.push_back(L"-e");
	wargs.push_back(bee::u2w(script));

	if (args.HasMember("arg0")) {
		if (args["arg0"].IsString()) {
			auto& v = args["arg0"];
			wargs.push_back(bee::u2w(std::string_view(v.GetString(), v.GetStringLength())));
		}
		else if (args["arg0"].IsArray()) {
			for (auto& v : args["arg0"].GetArray()) {
				if (v.IsString()) {
					wargs.push_back(bee::u2w(std::string_view(v.GetString(), v.GetStringLength())));
				}
			}
		}
	}

	std::wstring program = L".lua";
	if (args.HasMember("program") && args["program"].IsString()) {
		program = bee::u2w(std::string_view(args["program"].GetString(), args["program"].GetStringLength()));
	}
	wargs.push_back(program);

	if (args.HasMember("arg") && args["arg"].IsArray()) {
		for (auto& v : args["arg"].GetArray()) {
			if (v.IsString()) {
				wargs.push_back(bee::u2w(std::string_view(v.GetString(), v.GetStringLength())));
			}
		}
	}

	bee::subprocess::spawn spawn;
	spawn.set_console(bee::subprocess::console::eNew);
	spawn.hide_window();
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
	if (!replacedll.first.empty()) {
		spawn.suspended();
	}
	if (!spawn.exec(wargs, wcwd.c_str())) {
		return process_opt();
	}
	if (replacedll.first.empty()) {
		return bee::subprocess::process(spawn);
	}
	auto process = bee::subprocess::process(spawn);
	base::hook::replacedll(process, replacedll.first.c_str(), replacedll.second.c_str());
	process.resume();
	return process;
}
