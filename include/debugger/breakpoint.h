#pragma once

#include <map>
#include <string>
#include <deque>
#include <base/util/hybrid_array.h> 
#include <debugger/pathconvert.h>
#include <debugger/protocol.h>
#include <debugger/path.h>
#include <debugger/hashmap.h>

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

	enum class eLine : uint8_t {
		unknown = 0,
		undef = 1,
		defined = 2,
	};

	struct bp_source : public std::map<size_t, bp> {
		std::deque<eLine> defined;
		void update(lua_State* L, lua::Debug* ar);
	};

	struct bp_function {
		std::string clientpath;
		intptr_t sourceref;
		bp_source* bp;
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
		bool has(bp_source* src, size_t line, lua_State* L, lua::Debug* ar) const;
		bp_function* get_function(lua_State* L, lua::Debug* ar);
		bp_source& get_bp(intptr_t sourceref);
		bp_source& get_bp(const std::string& clientpath);
		pathconvert& get_pathconvert();

	private:
		void clear(bp_source& src);
		void add(bp_source& src, size_t line, rapidjson::Value const& bp);

	private:
		debugger_impl* dbg_;
		std::map<std::string, bp_source, path::less<std::string>> files_;
		std::map<intptr_t, bp_source>    memorys_;
		hashmap<bp_function>             functions_;
		base::hybrid_array<size_t, 1024> fast_table_;
	};
}
