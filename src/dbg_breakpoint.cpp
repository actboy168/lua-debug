#include "dbg_breakpoint.h"	
#include "dbg_evaluate.h"		
#include "dbg_path.h"
#include <lua.hpp>

namespace vscode
{
	breakpoint::breakpoint()
		: client_map_()
		, fast_table_()
	{
		fast_table_.fill(0);
	}

	void breakpoint::clear()
	{
		for (auto src : client_map_)
		{
			delete src.second;
		}
		client_map_.clear();
		fast_table_.clear();
	}

	void breakpoint::clear(const fs::path& client_path)
	{
		auto it = client_map_.find(client_path);
		if (it == client_map_.end())
		{
			return;
		}
		bp_source* src = it->second;
		for (auto line : *src)
		{
			fast_table_[line.first]--;
		}
		src->clear();
	}

	void breakpoint::insert(const fs::path& client_path, size_t line)
	{
		insert(client_path, line, std::string());
	}

	void breakpoint::insert(const fs::path& client_path, size_t line, const std::string& condition)
	{
		auto it = client_map_.find(client_path);
		if (it == client_map_.end())
		{
			client_map_.insert(std::make_pair(client_path, new bp_source(line, condition)));
		}
		else
		{
			bp_source* src = it->second;
			if (src->find(line) == src->end())
			{
				src->insert({ line, condition });
			}
			else
			{
				(*src)[line] = condition;
				return;
			}
		}

		if (line >= fast_table_.size())
		{
			size_t oldsize = fast_table_.size();
			size_t newsize = line + 1;
			fast_table_.resize(newsize);
			std::fill_n(fast_table_.begin() + oldsize, newsize - oldsize, 0);
		}
		fast_table_[line]++;
	}

	bool breakpoint::has(size_t line) const
	{
		if (line < fast_table_.size())
		{
			return fast_table_[line] > 0;
		}
		return false;
	}

	bool breakpoint::has(bp_source* src, size_t line, lua_State* L, lua_Debug* ar) const
	{
		auto it = src->find(line);
		if (it == src->end())
		{
			return false;
		}
		const std::string& condition = it->second;
		if (condition.empty())
		{
			return true;
		}
		int nresult = 0;
		if (!evaluate(L, ar, ("return " + condition).c_str(), nresult))
		{
			lua_pop(L, 1);
			return false;
		}
		if (nresult > 0 && lua_type(L, -nresult) == LUA_TBOOLEAN && lua_toboolean(L, -nresult))
		{
			return true;
		}
		lua_pop(L, nresult);
		return false;
	}

	bp_source* breakpoint::get(const std::string& server_path, pathconvert& pathconvert)
	{
		fs::path client_path;
		if (pathconvert.get(server_path, client_path))
		{
			auto it = client_map_.find(client_path);
			if (it != client_map_.end())
			{
				return it->second;
			}
		}
		return 0;
	}
}
