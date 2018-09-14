#include <Windows.h>
#include <debugger/client/run.h>
#include <debugger/client/stdinput.h>
#include <base/util/format.h>
#include <base/path/self.h>
#include <base/win/process.h>

std::string cmd_string(const std::string& str) {
	if (str.size() > 0 && str[str.size() - 1] == '\\' && (str.size() == 1 || str[str.size() - 2] != '\\')) {
		return "\"" + str + "\\\"";
	}
	else {
		return "\"" + str + "\"";
	}
}
std::wstring cmd_string(const std::wstring& str) {
	if (str.size() > 0 && str[str.size() - 1] == L'\\' && (str.size() == 1 || str[str.size() - 2] != L'\\')) {
		return L"\"" + str + L"\\\"";
	}
	else {
		return L"\"" + str + L"\"";
	}
}

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
	res += base::format("local dbg=package.loadlib([[%s]], 'luaopen_debugger')();package.loaded['debugger']=dbg;dbg:listen([[pipe:%s]]):guard()"
		, isUtf8 ? base::w2u((dbg_path / L"debugger.dll").wstring()) : base::w2a((dbg_path / L"debugger.dll").wstring())
		, base::w2u(port)
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
	res += ":start()";
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
	if (hExe == 0) {
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

bool create_luaexe_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port)
{
	auto& args = req["arguments"];
	base::win::process p;
	fs::path dbgPath = base::path::self().parent_path().parent_path();
	std::wstring luaexe;
	if (args.HasMember("luaexe") && args["luaexe"].IsString()) {
		luaexe = base::u2w(args["luaexe"].Get<std::string>());
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
				p.replace(base::u2w(args["luadll"].Get<std::string>()), "lua54.dll");
			}
		}
		else if (54032 == getLuaRuntime(args)) {
			dbgPath /= "x86";
			luaexe = dbgPath / "lua54.exe";
			if (args.HasMember("luadll") && args["luadll"].IsString()) {
				p.replace(base::u2w(args["luadll"].Get<std::string>()), "lua54.dll");
			}
		}
		else if (53064 == getLuaRuntime(args)) {
			dbgPath /= "x64";
			luaexe = dbgPath / "lua53.exe";
			if (args.HasMember("luadll") && args["luadll"].IsString()) {
				p.replace(base::u2w(args["luadll"].Get<std::string>()), "lua53.dll");
			}
		}
		else {
			dbgPath /= "x86";
			luaexe = dbgPath / "lua53.exe";
			if (args.HasMember("luadll") && args["luadll"].IsString()) {
				p.replace(base::u2w(args["luadll"].Get<std::string>()), "lua53.dll");
			}
		}
	}

	std::wstring wcommand = cmd_string(luaexe);
	std::wstring wcwd;
	if (args.HasMember("cwd") && args["cwd"].IsString()) {
		wcwd = base::u2w(args["cwd"].Get<std::string>());
	}
	else {
		wcwd = fs::path(luaexe).parent_path();
	}
	std::string script = create_install_script(req, dbgPath, port);

	wcommand += base::format(LR"( -e "%s")", base::u2w(script));
	if (args.HasMember("arg0")) {
		if (args["arg0"].IsString()) {
			auto& v = args["arg0"];
			wcommand += L" " + cmd_string(base::u2w(v));
		}
		else if (args["arg0"].IsArray()) {
			for (auto& v : args["arg0"].GetArray()) {
				if (v.IsString()) {
					wcommand += L" " + cmd_string(base::u2w(v));
				}
			}
		}
	}

	std::wstring program = L".lua";
	if (args.HasMember("program") && args["program"].IsString()) {
		program = base::u2w(args["program"]);
	}
	wcommand += L" " + cmd_string(program);

	if (args.HasMember("arg") && args["arg"].IsArray()) {
		for (auto& v : args["arg"].GetArray()) {
			if (v.IsString()) {
				wcommand += L" " + cmd_string(base::u2w(v));
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
