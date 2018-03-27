#include "dbg_pathconvert.h"  
#include "dbg_impl.h"
#include <base/util/unicode.h>
#include <base/filesystem.h>
#include <algorithm>
#include <assert.h>
#include <regex>
#include <deque>

namespace vscode
{
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

	std::string path_filename(const std::string& path)
	{
		for (ptrdiff_t pos = path.size() - 1; pos >= 0; --pos) {
			char c = path[pos];
			if (c == '\\' || c == '/') {
				return path.substr(pos + 1);
			}
		}
		return path;
	}
	pathconvert::pathconvert(debugger_impl* dbg, coding coding)
		: debugger_(dbg)
		, sourcemaps_()
		, coding_(coding)
	{ }

	void pathconvert::set_coding(coding coding)
	{
		coding_ = coding;
		source2clientpath_.clear();
	}

	void pathconvert::add_sourcemap(const std::string& srv, const std::string& cli)
	{
		sourcemaps_.push_back(std::make_pair(srv, cli));
	}

	void pathconvert::clear_sourcemap()
	{
		sourcemaps_.clear();
	}

	bool pathconvert::match_sourcemap(const std::string& srv, std::string& cli, const std::string& srvmatch, const std::string& climatch)
	{
		size_t i = 0;
		for (; i < srvmatch.size(); ++i) {
			if (i >= srv.size()) {
				return false;
			}
			if ((srvmatch[i] == '\\' || srvmatch[i] == '/') && (srv[i] == '\\' || srv[i] == '/')) {
				continue;
			}
			if (tolower((int)(unsigned char)srvmatch[i]) == tolower((int)(unsigned char)srv[i])) {
				continue;
			}
			return false;
		}
		cli = climatch + srv.substr(i);
		return true;
	}

	bool pathconvert::find_sourcemap(const std::string& srv, std::string& cli)
	{
		for (auto& it : sourcemaps_)
		{
			if (match_sourcemap(srv, cli, it.first, it.second))
			{
				return true;
			}
		}
		return false;
	}

	std::string pathconvert::source2serverpath(const std::string& s) const
	{
		fs::path path(coding_ == coding::utf8? base::u2w(s) : base::a2w(s));
		if (!path.is_absolute())
		{
			path = fs::absolute(path, fs::current_path());
		}
		return base::w2u(path_normalize(path).wstring());
	}

	bool pathconvert::get(const std::string& source, std::string& client_path)
	{
		auto it = source2clientpath_.find(source);
		if (it != source2clientpath_.end())
		{
			client_path = it->second;
			return true;
		}

		bool res = true;
		if (source[0] == '@')
		{
			std::string spath = source2serverpath(source.substr(1));
			std::string cpath;
			if (find_sourcemap(spath, cpath))
			{
				client_path = cpath;
			}
			else
			{
				client_path = spath;
			}
		}
		else if (source[0] == '=' && debugger_->custom_)
		{
			res = debugger_->custom_->path_convert(source, client_path);
		}
		else
		{
			client_path.clear();
			res = false;
		}
		source2clientpath_[source] = client_path;
		return res;
	}
}
