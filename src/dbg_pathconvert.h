#pragma once

#include "dbg_custom.h"
#include "dbg_path.h"
#include <map> 

namespace vscode
{
	class debugger_impl;

	class pathconvert
	{
	public:
		pathconvert(debugger_impl* dbg);
		void           set_script_path(const fs::path& path);
		bool           fget(const std::string& server_path, std::string*& client_path);
		custom::result eval(const std::string& server_path, std::string& client_path);
		custom::result get_or_eval(const std::string& server_path, std::string& client_path);

	private:
		debugger_impl*                     debugger_;
		std::map<std::string, std::string> server2client_;
		fs::path                           scriptpath_;
	};
}
