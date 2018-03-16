#include "dbg_pathconvert.h"
#include "dbg_path.h"	  
#include "dbg_impl.h"
#include "dbg_unicode.h"
#include <algorithm>
#include <assert.h>
#include <regex>

namespace vscode
{
	pathconvert::pathconvert(debugger_impl* dbg, coding coding)
		: debugger_(dbg)
		, sourcemaps_()
		, coding_(coding)
	{ }

	void pathconvert::set_coding(coding coding)
	{
		coding_ = coding;
		server2client_.clear();
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

	std::string pathconvert::source2path(const std::string& s) const
	{
		fs::path path(coding_ == coding::utf8? u2w(s) : a2w(s));
		if (!path.is_absolute())
		{
			path = fs::absolute(path, fs::current_path());
		}
		return w2u(path_normalize(path).wstring());
	}

	bool pathconvert::get(const std::string& server_path, std::string& client_path)
	{
		auto it = server2client_.find(server_path);
		if (it != server2client_.end())
		{
			client_path = it->second;
			return true;
		}

		bool res = true;
		if (server_path[0] == '@')
		{
			std::string spath = source2path(server_path.substr(1));
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
		else if (server_path[0] == '=')
		{
			if (debugger_->custom_) {
				res = debugger_->custom_->path_convert(server_path, client_path);
			}
		}
		else
		{
			client_path.clear();
			res = false;
		}
		server2client_[server_path] = client_path;
		return res;
	}
}
