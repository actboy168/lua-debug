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
		try {
			std::string re = srv;
			re = std::regex_replace(re, std::regex(R"(\?)"), R"((.*))");
			re = std::regex_replace(re, std::regex(R"(\\|/)"), R"([\\/])");
			sourcemaps_.push_back(std::make_pair(
				std::regex(re),
				cli
			));
		}
		catch (...) {
		}
	}

	void pathconvert::clear_sourcemap()
	{
		sourcemaps_.clear();
	}

	std::string pathconvert::server2u(const std::string& s) const
	{
		if (coding_ == coding::utf8) {
			return s;
		}
		return a2u(s);
	}

	bool pathconvert::find_sourcemap(const std::string& srv, std::string& cli)
	{
		for (auto& it : sourcemaps_)
		{
			try {
				std::smatch match;
				if (std::regex_match(srv, match, it.first))
				{
					if (match.size() == 2) {
						cli = std::regex_replace(it.second, std::regex("\\?"), match[1].str());
						return true;
					}
				}
			}
			catch (...) {

			}
		}
		return false;
	}

	fs::path pathconvert::server_complete(const fs::path& path)
	{
		if (path.is_absolute())
		{
			return path;
		}
		return fs::absolute(path, fs::current_path());
	}

	bool pathconvert::get(const std::string& server_path, fs::path& client_path)
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
			std::string spath = server2u(server_path.substr(1));
			std::string cpath;
			if (find_sourcemap(spath, cpath))
			{
				client_path = path_normalize(u2w(cpath));
			}
			else
			{
				client_path = path_normalize(server_complete(u2w(server2u(spath))));
			}
		}
		else if (server_path[0] == '=')
		{
			res = debugger_->custom_->path_convert(server_path, client_path);
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
