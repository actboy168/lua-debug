#include "utility.h"
#include <Windows.h>

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
