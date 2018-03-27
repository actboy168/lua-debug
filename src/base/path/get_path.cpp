#include <base/path/get_path.h>
#include <base/util/dynarray.h>

namespace base { namespace path {
	fs::path module(HMODULE module_handle)
	{
		wchar_t buffer[MAX_PATH];
		DWORD path_len = ::GetModuleFileNameW(module_handle, buffer, _countof(buffer));
		if (path_len == 0)
		{
			return fs::path();
		}

		if (path_len < _countof(buffer))
		{
			return std::move(fs::path(buffer, buffer + path_len));
		}

		for (size_t buf_len = 0x200; buf_len <= 0x10000; buf_len <<= 1)
		{
			std::dynarray<wchar_t> buf(path_len);
			path_len = ::GetModuleFileNameW(module_handle, buf.data(), (DWORD)buf.size());
			if (path_len == 0)
			{
				return fs::path();
			}

			if (path_len < _countof(buffer))
			{
				return std::move(fs::path(buf.begin(), buf.end()));
			}
		}

		return fs::path();
	}
}}
