#include "dbg_breakpoint.h"	
#include "dbg_evaluate.h"		
#include "dbg_path.h"
#include <lua.hpp>

namespace vscode
{
	breakpoint::breakpoint()
		: files_()
		, fast_table_()
	{
		fast_table_.fill(0);
	}

	void breakpoint::clear()
	{
		files_.clear();
		memorys_.clear();
		fast_table_.clear();
	}

	void breakpoint::clear(const fs::path& client_path)
	{
		auto it = files_.find(client_path);
		if (it != files_.end())
		{
			return clear(it->second);
		}
	}

	void breakpoint::clear(intptr_t source_ref)
	{
		auto it = memorys_.find(source_ref);
		if (it != memorys_.end())
		{
			return clear(it->second);
		}
	}

	void breakpoint::clear(bp_source& src)
	{
		for (auto line : src)
		{
			fast_table_[line.first]--;
		}
		src.clear();
	}

	void breakpoint::insert(const fs::path& client_path, size_t line, const std::string& condition)
	{
		return insert(files_[client_path], line, condition);
	}

	void breakpoint::insert(intptr_t source_ref, size_t line, const std::string& condition)
	{
		return insert(memorys_[source_ref], line, condition);
	}

	void breakpoint::insert(bp_source& src, size_t line, const std::string& condition)
	{
		if (src.find(line) != src.end())
		{
			src[line] = condition;
			return;
		}

		src.insert({ line, condition });
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

	bp_source* breakpoint::get(const char* source, pathconvert& pathconvert)
	{
		if (source[0] == '@' || source[0] == '=')
		{
			fs::path client_path;
			if (pathconvert.get(source, client_path))
			{
				auto it = files_.find(client_path);
				if (it != files_.end())
				{
					return &it->second;
				}
			}
		}
		else
		{
			auto it = memorys_.find((intptr_t)source);
			if (it != memorys_.end())
			{
				return &it->second;
			}
		}
		return nullptr;
	}
}
