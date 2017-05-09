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
	class bp_source
		: public std::map<size_t, std::string>
	{
	public:
		typedef std::map<size_t, std::string> base_type;

		bp_source(size_t line, const std::string& condition)
			: base_type()
		{ 
			insert(std::make_pair(line, condition));
		}
	};

	class breakpoint
	{
	public:
		typedef std::map<std::string, bp_source*> svr_map_type;
		typedef std::map<fs::path, bp_source*>    cli_map_type;

	public:
		breakpoint();
		void clear();
		void clear(const fs::path& client_path);
		void insert(const fs::path& client_path, size_t line);
		void insert(const fs::path& client_path, size_t line, const std::string& condition);
		bool has(size_t line) const;
		bool has(bp_source* src, size_t line, lua_State* L, lua_Debug* ar) const;
		bp_source* get(const std::string& server_path, pathconvert& pathconvert);

	private:
		svr_map_type              server_map_;
		cli_map_type              client_map_;
		hybridarray<size_t, 1024> fast_table_;
	};
}
