#pragma once

#include <map>
#include <string>
#include <base/util/hybrid_array.h> 
#include "dbg_pathconvert.h"

struct lua_State;
namespace lua { union Debug; }

namespace vscode
{
	struct bp {
		std::string condition;
		std::string hitcondition;
		int hit = 0;

	};
	typedef std::map<size_t, bp> bp_source;

	class breakpoint
	{
	public:
		breakpoint();
		void clear();
		void clear(const std::string& client_path);
		void clear(intptr_t source_ref);
		void add(const std::string& client_path, size_t line, const std::string& condition, const std::string& hitcondition);
		void add(intptr_t source_ref, size_t line, const std::string& condition, const std::string& hitcondition);
		bool has(size_t line) const;
		bool has(bp_source* src, size_t line, lua_State* L, lua::Debug* ar) const;
		bp_source* get(const char* source, pathconvert& pathconvert);

	private:
		void clear(bp_source& src);
		void add(bp_source& src, size_t line, const std::string& condition, const std::string& hitcondition);
		bool evaluate_isok(lua_State* L, lua::Debug *ar, const std::string& script) const;

	private:
		std::map<std::string, bp_source> files_;
		std::map<intptr_t, bp_source>    memorys_;
		base::hybrid_array<size_t, 1024> fast_table_;
	};
}
