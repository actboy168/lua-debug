#pragma once

#include "dbg_custom.h"
#include "dbg_path.h"
#include "debugger.h"
#include <map> 	   
#include <vector> 

namespace vscode
{
	class debugger_impl;

	class pathconvert
	{
	public:
		typedef custom::sourcemap_t sourcemap_t;

	public:
		pathconvert(debugger_impl* dbg, coding coding);
		void   add_sourcemap(const std::string& srv, const std::string& cli);
		void   clear_sourcemap();
		bool   find_sourcemap(const std::string& srv, std::string& cli);
		bool   get(const std::string& server_path, fs::path& client_path);
		void   set_coding(coding coding);

	private:
		fs::path server_complete(const fs::path& path);
		std::string server2u(const std::string& s) const;

	private:
		debugger_impl*                     debugger_;
		std::map<std::string, fs::path>    server2client_;
		sourcemap_t                        sourcemaps_;
		coding                             coding_;
	};
}
