#include "dbg_pathconvert.h"
#include "dbg_path.h"	  
#include "dbg_impl.h"
#include <algorithm>
#include <assert.h>

namespace vscode
{
	pathconvert::pathconvert(debugger_impl* dbg)
		: debugger_(dbg)
		, scriptpath_(fs::current_path<fs::path>())
	{ }

	void pathconvert::set_script_path(const fs::path& path)
	{
		scriptpath_ = path;
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
			std::error_code ec;
			client_path = path_uncomplete(path, scriptpath_, ec).file_string();
			assert(!ec);
			server2client_[server_path] = client_path;
			return custom::result::sucess;
		}
		else if (server_path[0] == '=')
		{
			custom::result r = debugger_->custom_->path_convert(server_path, client_path);
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
