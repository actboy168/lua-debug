#include <base/path/helper.h>
#include <deque>
#include <algorithm>

namespace base { namespace path {
	fs::path normalize(const fs::path& p)
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
}}
