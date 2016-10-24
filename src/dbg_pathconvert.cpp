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

	void pathconvert::add_sourcemap(const std::string& srv, const std::string& cli)
	{
		fs::path srvpath = srv;
		if (!srvpath.is_complete())
		{
			srvpath = fs::complete(srvpath, fs::current_path<fs::path>());
		}
		sourcemaps_.push_back(std::make_pair(srvpath, cli));
	}

	bool pathconvert::fget(const std::string& server_path, std::string*& client_path)
	{
		auto it = server2client_.find(server_path);
		if (it != server2client_.end())
		{
			client_path = &it->second;
			return true;
		}
		return false;
	}

	custom::result pathconvert::eval(const std::string& server_path, std::string& client_path)
	{
		if (server_path[0] == '@')
		{
			std::string path;
			path.resize(server_path.size() - 1);
			std::transform(server_path.begin() + 1, server_path.end(), path.begin(), tolower);

			fs::path srvpath = path;
			client_path = srvpath;
			for (auto& pair : sourcemaps_)
			{
				if (path_is_subpath(srvpath, pair.first))
				{
					client_path = pair.second;
				}
			}
			server2client_[server_path] = client_path;
			return custom::result::sucess;
		}
		else if (server_path[0] == '=')
		{
			custom::result r = debugger_->custom_->path_convert(server_path, client_path, sourcemaps_);
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
		return custom::result::sucess;
	}

	custom::result pathconvert::get_or_eval(const std::string& server_path, std::string& client_path)
	{
		std::string* ptr;
		if (fget(server_path, ptr))
		{
			client_path = *ptr;
			return custom::result::sucess;
		}
		return eval(server_path, client_path);
	}
}
