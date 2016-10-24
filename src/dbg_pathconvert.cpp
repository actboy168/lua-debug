#include "dbg_pathconvert.h"
#include "dbg_path.h"	  
#include "dbg_impl.h"
#include <algorithm>
#include <assert.h>

namespace vscode
{
	pathconvert::pathconvert(debugger_impl* dbg)
		: debugger_(dbg)
		, sourcemaps_()
	{ }

	void pathconvert::add_sourcemap(const fs::path& srv, const fs::path& cli)
	{
		if (srv.is_complete())
		{
			sourcemaps_.push_back(std::make_pair(path_normalize(srv), cli));
		}
		else
		{
			sourcemaps_.push_back(std::make_pair(path_normalize(fs::complete(srv, fs::current_path<fs::path>())), cli));
		}
	}

	void pathconvert::clear_sourcemap()
	{
		sourcemaps_.clear();
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
		if (path.is_complete())
		{
			return path;
		}
		return path_normalize(fs::complete(path, fs::current_path<fs::path>()));
	}

	pathconvert::result pathconvert::eval_uncomplete(const std::string& server_path, fs::path& client_path)
	{
		if (server_path[0] == '@')
		{
			fs::path srvpath = server_complete(server_path.substr(1));
			for (auto& pair : sourcemaps_)
			{
				if (path_is_subpath(srvpath, pair.first))
				{
					std::error_code ec;
					client_path = path_uncomplete(srvpath, pair.first, ec);
					assert(!ec);
					return result::sucess;
				}
			}
			std::error_code ec;
			client_path = path_uncomplete(srvpath, fs::current_path<fs::path>(), ec);
			assert(!ec);
			return result::sucess;
		}
		else if (server_path[0] == '=')
		{
			return debugger_->custom_->path_convert(server_path, client_path, sourcemaps_, true);
		}
		client_path = server_path;
		return result::sucess;
	}


	pathconvert::result pathconvert::eval(const std::string& server_path, fs::path& client_path)
	{
		if (server_path[0] == '@')
		{
			fs::path srvpath = server_complete(server_path.substr(1));
			for (auto& pair : sourcemaps_)
			{
				if (path_is_subpath(srvpath, pair.first))
				{
					std::error_code ec;
					client_path = path_normalize(pair.second / path_uncomplete(srvpath, pair.first, ec));
					server2client_[server_path] = client_path;
					assert(!ec);
					return result::sucess;
				}
			}
			client_path = path_normalize(srvpath);
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
		client_path = server_path;
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
