
#include <base/win/process.h>
#include "dbg_protocol.h"
#include <base/util/unicode.h>

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

fs::path get_self_path()
{
	HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
	wchar_t buffer[MAX_PATH];
	DWORD len = ::GetModuleFileNameW(module, buffer, _countof(buffer));
	if (len == 0)
	{
		return fs::path();
	}
	if (len < _countof(buffer))
	{
		return fs::path(buffer, buffer + len);
	}
	for (size_t buf_len = 0x200; buf_len <= 0x10000; buf_len <<= 1)
	{
		std::unique_ptr<wchar_t[]> buf(new wchar_t[len]);
		len = ::GetModuleFileNameW(module, buf.get(), len);
		if (len == 0)
		{
			return fs::path();
		}
		if (len < _countof(buffer))
		{
			return fs::path(buf.get(), buf.get() + (size_t)len);
		}
	}
	return fs::path();
}


bool create_process_with_debugger(vscode::rprotocol& req, uint16_t port)
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
	auto dir = get_self_path().remove_filename().remove_filename();
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
	p.set_env(L"LUADBG_PORT", std::to_wstring(port));
	return p.create(wapplication.c_str(), wcommand.c_str(), wcwd.c_str());
}
