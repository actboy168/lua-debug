#pragma once

#include <debugger/debugger.h>
#include <debugger/config.h>
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
		void        initialize(config& config);
		void        setSourceCoding(eCoding coding);
		bool        path_convert(const std::string& source, std::string& client);
		std::string path_exception(const std::string& str);
		std::string path_clientrelative(const std::string& path);

	private:
		bool        source2server(const std::string& source, std::string& server);
		bool        server2client(const std::string& server, std::string& client);

	private:
		debugger_impl*                     debugger_;
		std::map<std::string, std::string> source2client_;
		sourcemap_t                        sourceMap_;
		skipfiles_t                        skipFiles_;
		eCoding                            sourceCoding_;
		std::string                        workspaceRoot_;
	};
}
