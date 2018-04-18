#pragma once

#include <map>
#include <string>
#include <base/util/hybrid_array.h> 
#include <debugger/pathconvert.h>
#include <debugger/protocol.h>
#include <debugger/path.h>

struct lua_State;
namespace lua { struct Debug; }

namespace vscode
{
	class breakpoint;

	struct bp {
		std::string cond;
		std::string hitcond;
		std::string log;
		int hit;
		bp(rapidjson::Value const& info, int h);

	};
	typedef std::map<size_t, bp> bp_source;

	struct bp_function {
		std::string path;
		intptr_t    sourceref;
		bp_source*  bp;
		bp_function(lua_State* L, lua::Debug* ar, breakpoint* breakpoint);
	};

	class breakpoint
	{
	public:
		breakpoint(debugger_impl* dbg);
		void clear();
		void clear(const std::string& client_path);
		void clear(intptr_t source_ref);
		void add(const std::string& client_path, size_t line, rapidjson::Value const& bp);
		void add(intptr_t source_ref, size_t line, rapidjson::Value const& bp);
		bool has(size_t line) const;
		bool has(bp_source* src, size_t line, lua_State* L, lua::Debug* ar) const;
		bp_source* get(const char* source);
		bp_source* get(lua_State* L, lua::Debug* ar);

	private:
		void clear(bp_source& src);
		void add(bp_source& src, size_t line, rapidjson::Value const& bp);

	private:
		debugger_impl* dbg_;
		std::map<std::string, bp_source, path::less<std::string>> files_;
		std::map<intptr_t, bp_source>    memorys_;
		std::map<intptr_t, bp_function>  functions_;
		base::hybrid_array<size_t, 1024> fast_table_;
	};
}
