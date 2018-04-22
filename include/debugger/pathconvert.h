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
		typedef std::vector<std::string>                         skipfiles_t;

	public:
		pathconvert(debugger_impl* dbg);
		void   add_sourcemap(const std::string& server, const std::string& client);
		void   add_skipfiles(const std::string& pattern);
		void   clear();
		bool   server2client(const std::string& server, std::string& client);
		bool   get(const std::string& source, std::string& client);
		void   set_coding(eCoding coding);
		std::string exception(const std::string& str);

	private:
		bool match_sourcemap(const std::string& srv, std::string& cli, const std::string& srvmatch, const std::string& climatch);

	private:
		debugger_impl*                     debugger_;
		std::map<std::string, std::string> source2client_;
		sourcemap_t                        sourcemap_;
		skipfiles_t                        skipfiles_;
		eCoding                            coding_;
	};
}
