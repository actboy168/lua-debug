#pragma once

#include "dbg_custom.h"
#include "dbg_path.h"
#include <map> 	   
#include <vector> 

namespace vscode
{
	class debugger_impl;

	class pathconvert
	{
		typedef std::vector<std::pair<std::string, std::string>> sourcemap_t;

	public:
		pathconvert(debugger_impl* dbg);
		void           add_sourcemap(const std::string& srv, const std::string& cli);
		bool           fget(const std::string& server_path, std::string*& client_path);
		custom::result eval(const std::string& server_path, std::string& client_path);
		custom::result get_or_eval(const std::string& server_path, std::string& client_path);

	private:
		debugger_impl*                     debugger_;
		std::map<std::string, std::string> server2client_;
		sourcemap_t                        sourcemaps_;
	};
}
