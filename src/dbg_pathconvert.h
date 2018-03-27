#pragma once

#include "debugger.h"
#include <map>
#include <vector>

namespace vscode
{
	class debugger_impl;

	class pathconvert
	{
	public:
		typedef std::vector<std::pair<std::string, std::string>> sourcemap_t;

	public:
		pathconvert(debugger_impl* dbg);
		void   add_sourcemap(const std::string& srv, const std::string& cli);
		void   clear_sourcemap();
		bool   find_sourcemap(const std::string& srv, std::string& cli);
		bool   get(const std::string& server_path, std::string& client_path);
		void   set_coding(coding coding);

	private:
		std::string source2serverpath(const std::string& s) const;
		bool match_sourcemap(const std::string& srv, std::string& cli, const std::string& srvmatch, const std::string& climatch);

	private:
		debugger_impl*                     debugger_;
		std::map<std::string, std::string> source2clientpath_;
		sourcemap_t                        sourcemaps_;
		coding                             coding_;
	};

	std::string path_filename(const std::string& path);
}
