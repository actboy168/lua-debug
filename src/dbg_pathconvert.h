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
		typedef custom::sourcemap_t sourcemap_t;
		typedef custom::result      result;

	public:
		pathconvert(debugger_impl* dbg);
		void   add_sourcemap(const fs::path& srv, const fs::path& cli);
		bool   fget(const std::string& server_path, fs::path*& client_path);
		result eval(const std::string& server_path, fs::path& client_path);
		result get_or_eval(const std::string& server_path, fs::path& client_path);
		result eval_uncomplete(const std::string& server_path, fs::path& client_path);

	private:
		fs::path server_complete(const fs::path& path);

	private:
		debugger_impl*                     debugger_;
		std::map<std::string, fs::path>    server2client_;
		sourcemap_t                        sourcemaps_;
		fs::path                           currentpath_;
	};
}
