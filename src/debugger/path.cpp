#include <debugger/path.h>
#include <bee/utility/unicode.h>
#include <bee/utility/dynarray.h>
#include <deque>
#if defined(_WIN32)
#include <Windows.h>
#else
#include <unistd.h>
#include <memory>
#endif

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
#if defined(_WIN32)
		DWORD len = GetCurrentDirectoryW(0, nullptr);
		if (len == 0) {
			return std::string("c:");
		}
		std::dynarray<wchar_t> r(len);
		len = GetCurrentDirectoryW(len, r.data());
		return bee::w2u(std::wstring_view(r.data(), len));
#else
		for (long path_max = 128;; path_max *= 2)
		{
			std::unique_ptr<char[]> buf(new char[static_cast<std::size_t>(path_max)]);
			if (::getcwd(buf.get(), static_cast<std::size_t>(path_max)) == 0) {
				if (errno != ERANGE) {
					break;
				}
			}
			else {
				return buf.get();
			}
		}
		return std::string("~/");
#endif
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

	static std::string path_create(const std::string& path, const std::deque<std::string>& stack, char sep)
	{
		std::string result = path;
		for (auto& e : stack) {
			result += sep + e;
		}
		return result;
	}

	template <typename EQUAL>
	bool glob_match(const std::string_view& pattern, const std::string_view& target, EQUAL equal)
	{
		auto p = pattern.data();
		auto pe = pattern.data() + pattern.size();
		auto q = target.data();
		auto qe = target.data() + target.size();
		for (;;) {
			if (p == pe) {
				return q == qe;
			}
			if (*p == '*') {
				++p;
				for (auto backtracker = qe; backtracker >= q; --backtracker) {
					if (glob_match(std::string_view(p, pe - p), std::string_view(backtracker, qe - backtracker), equal)) {
						return true;
					}
				}
				break;
			}
			if (q == qe) {
				break;
			}
			if (*p != '?' && !equal(*p, *q)) {
				break;
			}
			++p, ++q;
		}
		return false;
	}

	int tochar(char c) {
		if (c == '/') return '\\';
		return tolower((int)(unsigned char)c);
	}

	std::string normalize(const std::string& path, char sep)
	{
		std::deque<std::string> stack;
		std::string result = path_normalize(path, stack);
		return path_create(result, stack, sep);
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

	bool glob_match(const std::string& pattern, const std::string& target)
	{
		return glob_match(pattern, target, [](char a, char b) { return tochar(a) == tochar(b); });
	}

	std::string relative(const std::string& path, const std::string& base, char sep)
	{
		std::deque<std::string> spath, sbase;
		std::string rpath = path_normalize(path, spath);
		std::string rbase = path_normalize(base, sbase);
		if (rpath != rbase) {
			return path_create(rpath, spath, sep);
		}
		while (!spath.empty() && !sbase.empty() && spath.front() == sbase.front()) {
			spath.pop_front();
			sbase.pop_front();
		}
		if (spath.empty() && sbase.empty()) {
			return "." + sep;
		}
		std::string result = "";
		for (auto& e : sbase) {
			result += sep + "..";
		}
		for (auto& e : spath) {
			result += sep + e;
		}
		return result.substr(1);
	}
}}
