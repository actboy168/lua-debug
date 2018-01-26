#pragma once

#include <map> 
#include <set>
#include <string>				
#include "dbg_hybridarray.h"  
#include "dbg_pathconvert.h"

struct lua_State;
struct lua_Debug;

namespace vscode
{
	typedef std::map<size_t, std::string> bp_source;

	class breakpoint
	{
	public:
		breakpoint();
		void clear();
		void clear(const fs::path& client_path);
		void clear(intptr_t source_ref);
		void insert(const fs::path& client_path, size_t line, const std::string& condition);
		void insert(intptr_t source_ref, size_t line, const std::string& condition);
		bool has(size_t line) const;
		bool has(bp_source* src, size_t line, lua_State* L, lua_Debug* ar) const;
		bp_source* get(const char* source, pathconvert& pathconvert);

	private:
		void clear(bp_source& src);
		void insert(bp_source& src, size_t line, const std::string& condition);

	private:
		std::map<fs::path, bp_source> files_;
		std::map<intptr_t, bp_source> memorys_;
		hybridarray<size_t, 1024>     fast_table_;
	};
}
