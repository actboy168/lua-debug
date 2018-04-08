#pragma once

#include <debugger/debugger.h>
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
		std::string find_sourcemap(const std::string& srv);
		bool   get(const std::string& server_path, std::string& client_path);
		void   set_coding(coding coding);

	private:
		bool match_sourcemap(const std::string& srv, std::string& cli, const std::string& srvmatch, const std::string& climatch);

	private:
		debugger_impl*                     debugger_;
		std::map<std::string, std::string> source2path_;
		sourcemap_t                        sourcemaps_;
		coding                             coding_;
	};
}
