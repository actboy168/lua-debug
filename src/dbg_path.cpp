#include "dbg_path.h" 
#include <deque>
#include <algorithm>

namespace vscode
{
	fs::path path_uncomplete(const fs::path& path, const fs::path& base, std::error_code& ec)
	{
		fs::path npath = path_normalize(path);
		fs::path nbase = path_normalize(base);
		if (npath == nbase)
			return L"./";
		fs::path from_path, from_base, output;
		fs::path::iterator path_it = npath.begin(), path_end = npath.end();
		fs::path::iterator base_it = nbase.begin(), base_end = nbase.end();

		if ((path_it == path_end) || (base_it == base_end))
		{
			ec = std::make_error_code(std::errc::not_a_directory);
			return npath;
		}

#ifdef WIN32
		if (*path_it != *base_it)
			return npath;
		++path_it, ++base_it;
#endif

		while (true) {
			if ((path_it == path_end) || (base_it == base_end) || (*path_it != *base_it)) {
				for (; base_it != base_end; ++base_it) {
					if (*base_it == L".")
						continue;
					else if (*base_it == L"/")
						continue;

					output /= L"../";
				}
				fs::path::iterator path_it_start = path_it;
				for (; path_it != path_end; ++path_it) {

					if (path_it != path_it_start)
						output /= L"/";

					if (*path_it == L".")
						continue;
					if (*path_it == L"/")
						continue;

					output /= *path_it;
				}

				break;
			}
			from_path /= fs::path(*path_it);
			from_base /= fs::path(*base_it);
			++path_it, ++base_it;
		}

		return output;
	}

	fs::path path_normalize(const fs::path& p)
	{
		fs::path result = p.root_path();
		std::deque<std::wstring> stack;
		for (auto e : p.relative_path()) {
			if (e == L".." && !stack.empty() && stack.back() != L"..") {
				stack.pop_back();
			}
			else if (e != L".") {
#if _MSC_VER >= 1900
				stack.push_back(e.wstring());
#else
				stack.push_back(e);
#endif
			}
		}
		for (auto e : stack) {
			result /= e;
		}
		return result.wstring();
	}

	bool path_is_subpath(const fs::path& path, const fs::path& base)
	{
		fs::path npath = path_normalize(path);
		fs::path::iterator path_it = npath.begin(), path_end = npath.end();
		fs::path::iterator base_it = base.begin(), base_end = base.end();
		
		for (;;)
		{
			if (base_it == base_end) {
				return true;
			}
			if (path_it == path_end) {
				return false;
			}
			if (*path_it != *base_it) {
				return false;
			}
			++path_it;
			++base_it;
		}
		return false;
	}
}
