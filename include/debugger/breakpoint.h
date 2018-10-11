#pragma once

#include <map>
#include <string>
#include <deque>
#include <vector>
#include <base/util/hybrid_array.h>
#include <debugger/protocol.h>
#include <debugger/path.h>
#include <debugger/hashmap.h>

struct lua_State;
namespace lua { struct Debug; }

namespace vscode
{
	class debugger_impl;
	class breakpoint;
	struct bp_source;
	struct source;

	struct bp_breakpoint {
		size_t id;
		size_t line;
		bool verified;

		std::string cond;
		std::string hitcond;
		std::string log;
		int hit;

		bp_breakpoint(size_t id, rapidjson::Value const& info);
		bool verify(bp_source& src, debugger_impl* dbg = nullptr);
		bool run(lua_State* L, lua::Debug* ar, debugger_impl& dbg);
		void output(wprotocol& res);
		void update(rapidjson::Value const& info);
	};

	enum class eLine : uint8_t {
		unknown = 0,
		undef = 1,
		defined = 2,
	};

	struct bp_source {
		std::deque<eLine>               defined;
		std::vector<bp_breakpoint>      waitverfy;
		std::map<size_t, bp_breakpoint> verified;
		debugger_impl&                  dbg;

		bp_source(debugger_impl& dbg);
		bp_source(bp_source&& s);
		~bp_source();
		void update(lua_State* L, lua::Debug* ar);

		bp_breakpoint& add(size_t line, rapidjson::Value const& bpinfo, size_t& next_id);
		bp_breakpoint* get(size_t line);
		void clear(rapidjson::Value const& args);
		bool has_breakpoint();

		bp_source(bp_source&) = delete;
		bp_source& operator=(bp_source&) = delete;
	};

	class breakpoint
	{
	public:
		breakpoint(debugger_impl& dbg);
		void clear();
		bool has(bp_source* src, size_t line, lua_State* L, lua::Debug* ar) const;
		bp_source*   get_function(lua_State* L, lua::Debug* ar);
		bp_source&   get_source(source& source);
		void set_breakpoint(source& s, rapidjson::Value const& args, wprotocol& res);
		
	private:
		debugger_impl& dbg_;
		std::map<std::string, bp_source, path::less<std::string>> files_;
		std::map<intptr_t, bp_source>    memorys_;
		hashmap<bp_source>               functions_;
		size_t                           next_id_;
	};
}
