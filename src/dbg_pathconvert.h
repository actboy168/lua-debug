#pragma once

#include "dbg_custom.h"
#include <map> 

namespace vscode
{
	class dbg_custom;

	class pathconvert
	{
	public:
		bool           fget(const std::string& server_path, std::string*& client_path);
		custom::result eval(const std::string& server_path, std::string& client_path, custom& custom);
		custom::result get_or_eval(const std::string& server_path, std::string& client_path, custom& custom);

	private:
		std::map<std::string, std::string> server2client_;
	};
}
