#include <debugger/path.h>
#include <base/util/unicode.h>
#include <deque>
#include <Windows.h>

namespace vscode { namespace path {
	static bool is_sep(char c)
	{
		return c == '/' || c == '\\';
	}

	static void path_push(std::deque<std::string>& stack, const std::string& path)
	{
		if (path.empty()) {
			// do nothing
		}
		else if (path == ".." && !stack.empty() && stack.back() != "..") {
			stack.pop_back();
		}
		else if (path != ".") {
			stack.push_back(path);
		}
	}

	static std::string path_currentpath()
	{
		DWORD len = GetCurrentDirectoryW(0, nullptr);
		if (len == 0) {
			return std::string("c:");
		}
		std::wstring r;
		r.resize(len);
		GetCurrentDirectoryW((DWORD)r.size(), &r.front());
		return base::w2u(r);
	}

	static std::string path_normalize(const std::string& path, std::deque<std::string>& stack)
	{
		std::string root;
		size_t pos = path.find(':', 0);
		if (pos == path.npos) {
			root = path_normalize(path_currentpath(), stack);
			pos = 0;
		}
		else {
			pos++;
			root = path.substr(0, pos);
		}

		for (size_t i = pos; i < path.size(); ++i) {
			char c = path[i];
			if (is_sep(c)) {
				if (i > pos) {
					path_push(stack, path.substr(pos, i - pos));
				}
				pos = i + 1;
			}
		}
		path_push(stack, path.substr(pos));
		return root;
	}

	std::string normalize(const std::string& path)
	{
		std::deque<std::string> stack;
		std::string result = path_normalize(path, stack);
		for (auto& e : stack) {
			result += '\\' + e;
		}
		return result;
	}

	std::string filename(const std::string& path)
	{
		for (ptrdiff_t pos = path.size() - 1; pos >= 0; --pos) {
			char c = path[pos];
			if (is_sep(c)) {
				return path.substr(pos + 1);
			}
		}
		return path;
	}

	int tochar(char c) {
		if (c == '/') return '\\';
		return tolower((int)(unsigned char)c);
	}
}}
