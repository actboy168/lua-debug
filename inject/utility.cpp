#include "utility.h"
#include <Windows.h>
#include <base/hook/detail/import_address_table.h>

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

const char* search_api(const char* api1, const char* api2)
{
	base::hook::detail::import_address_table iat;
	if (!iat.open_module(GetModuleHandleW(NULL))) {
		return nullptr;
	}
	const char* res = nullptr;
	bool ok = iat.each_dll([&](const char* dllname)->bool{
		if (iat.get_proc_address(api1)) {
			res = dllname;
			return true;
		}
		if (iat.get_proc_address(api2)) {
			res = dllname;
			return true;
		}
		return false;
	});
	if (!ok) {
		return nullptr;
	}
	return res;
}
