#include "dbg_pathconvert.h"
#include "dbg_path.h"
#include <algorithm>
#include <assert.h>

namespace vscode
{
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

	custom::result pathconvert::eval(const std::string& server_path, std::string& client_path, custom& custom)
	{
		if (server_path[0] == '@')
		{
			std::string path;
			path.resize(server_path.size() - 1);
			std::transform(server_path.begin() + 1, server_path.end(), path.begin(), tolower);
			std::error_code ec;
			client_path = path_uncomplete(path, fs::current_path<fs::path>(), ec).file_string();
			assert(!ec);
			server2client_[server_path] = client_path;
			return custom::result::sucess;
		}
		else if (server_path[0] == '=')
		{
			custom::result r = custom.path_convert(server_path, client_path);
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

	custom::result pathconvert::get_or_eval(const std::string& server_path, std::string& client_path, custom& custom)
	{
		std::string* ptr;
		if (fget(server_path, ptr))
		{
			client_path = *ptr;
			return custom::result::sucess;
		}
		return eval(server_path, client_path, custom);
	}
}
