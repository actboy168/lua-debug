#pragma once 

#include <debugger/breakpoint.h>

namespace vscode {
	class debugger_impl;

	struct source {
		bool valid = false;
		std::string path;
		intptr_t ref = 0;

		void output(wprotocol& res);

	private:
		friend class sourceMgr;
		source();
	};

	class sourceMgr {
	public:
		sourceMgr(debugger_impl& dbg);
		source* create(lua::Debug* ar);
		source* create(rapidjson::Value const& info);
		source* open(rapidjson::Value const& info);
		void    loadedSources(wprotocol& res);

	private:
		source* createByPath(const std::string& path);
		source* createByRef(int64_t ref);
		source* openByPath(const std::string& path);
		source* openByRef(int64_t ref);

	private:
		debugger_impl& dbg_;
		std::map<std::string, source, path::less<std::string>> poolPath_;
		std::map<int64_t, source>                              poolRef_;
	};
}
