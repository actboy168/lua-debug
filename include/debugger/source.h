#pragma once 

#include <debugger/breakpoint.h>

namespace vscode {
	class debugger_impl;

	struct source {
		bool valid = false;
		std::string path;
		std::string name;
		uint32_t ref = 0;
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
		source* createByRef(const std::string& code);
		source* open(rapidjson::Value const& info);
		void    loadedSources(wprotocol& res);
		bool    getCode(uint32_t ref, std::string& code);

	private:
		source*  createByPath(const std::string& path);
		source*  openByPath(const std::string& path);
		source*  openByRef(uint32_t ref);
		uint32_t codeHash(const std::string& s);

	private:
		debugger_impl& dbg_;
		std::map<std::string, source, path::less<std::string>> poolPath_;
		std::map<uint32_t, source>                             poolRef_;
		std::map<uint32_t, std::string>                        poolCode_;
	};
}
