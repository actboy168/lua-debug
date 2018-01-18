#include "dbg_pathconvert.h"
#include "dbg_path.h"	  
#include "dbg_impl.h"
#include "dbg_unicode.h"
#include <algorithm>
#include <assert.h>
#include <regex>

namespace vscode
{
	pathconvert::pathconvert(debugger_impl* dbg)
		: debugger_(dbg)
		, sourcemaps_()
	{ }

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

	bool pathconvert::fget(const std::string& server_path, fs::path*& client_path)
	{
		auto it = server2client_.find(server_path);
		if (it != server2client_.end())
		{
			client_path = &it->second;
			return true;
		}
		return false;
	}

	fs::path pathconvert::server_complete(const fs::path& path)
	{
		if (path.is_absolute())
		{
			return path;
		}
		return path_normalize(fs::absolute(path, fs::current_path()));
	}

	pathconvert::result pathconvert::eval_uncomplete(const std::string& server_path, fs::path& client_path)
	{
		if (server_path[0] == '@')
		{
			std::string spath = server_path.substr(1);
			std::string cpath;
			if (find_sourcemap(spath, cpath))
			{
				client_path = path_normalize(u2w(cpath));
				return result::sucess;
			}
			client_path = server_complete(fs::path(a2w(spath)));
			return result::sucess;
		}
		else if (server_path[0] == '=')
		{
			return debugger_->custom_->path_convert(server_path, client_path, sourcemaps_, true);
		}
		client_path = fs::path(a2w(server_path));
		return result::sucess;
	}

	pathconvert::result pathconvert::eval(const std::string& server_path, fs::path& client_path)
	{
		if (server_path[0] == '@')
		{
			std::string spath = server_path.substr(1);
			std::string cpath;
			if (find_sourcemap(spath, cpath))
			{
				client_path = path_normalize(u2w(cpath));
				server2client_[server_path] = client_path;
				return result::sucess;
			}
			client_path = server_complete(fs::path(a2w(spath)));
			server2client_[server_path] = client_path;
			return result::sucess;
		}
		else if (server_path[0] == '=')
		{
			result r = debugger_->custom_->path_convert(server_path, client_path, sourcemaps_, false);
			switch (r)
			{
			case custom::result::failed:
			case custom::result::sucess:
				server2client_[server_path] = client_path;
				break;
			}
			return r;
		}
		client_path = fs::path(a2w(server_path));
		server2client_[server_path] = client_path;
		return result::sucess;
	}

	pathconvert::result pathconvert::get_or_eval(const std::string& server_path, fs::path& client_path)
	{
		fs::path* ptr;
		if (fget(server_path, ptr))
		{
			client_path = *ptr;
			return result::sucess;
		}
		return eval(server_path, client_path);
	}
}
