#pragma once

#include "dbg_custom.h"
#include "dbg_path.h"
#include <map> 

namespace vscode
{
	class dbg_custom;

	class pathconvert
	{
	public:
		pathconvert();
		void           set_script_path(const fs::path& path);
		bool           fget(const std::string& server_path, std::string*& client_path);
		custom::result eval(const std::string& server_path, std::string& client_path, custom& custom);
		custom::result get_or_eval(const std::string& server_path, std::string& client_path, custom& custom);

	private:
		std::map<std::string, std::string> server2client_;
		fs::path                           scriptpath_;
	};
}
