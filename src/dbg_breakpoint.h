#pragma once

#include <map> 
#include <set>
#include <string>				
#include "dbg_hybridarray.h"  
#include "dbg_evaluate.h"

namespace vscode
{
	class breakpoint
		: private std::map<std::string, std::map<size_t, std::string>>
	{
	public:
		typedef std::map<std::string, std::map<size_t, std::string>> base_type;

	public:
		breakpoint()
			: base_type()
			, fast_table_()
		{
			fast_table_.fill(0);
		}

		void clear()
		{
			base_type::clear();
			fast_table_.clear();
		}

		void clear(const std::string& path)
		{
			auto it = find(path);
			if (it == end())
			{
				return;
			}
			auto& lines = it->second;
			for (auto line : lines)
			{
				fast_table_[line.first]--;
			}
			lines.clear();
		}
		
		void insert(const std::string& path, size_t line)
		{
			insert(path, line, std::string());
		}

		void insert(const std::string& path, size_t line, const std::string& condition)
		{
			auto it = find(path);
			if (it == end())
			{
				base_type::insert(std::make_pair(path, std::map<size_t, std::string> { { line, condition } }));
			}
			else
			{
				auto& lines = it->second;
				if (lines.find(line) == lines.end())
				{
					lines.insert({ line, condition });
				}
				else
				{
					lines[line] = condition;
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

		bool has(size_t line) const
		{
			if (line < fast_table_.size())
			{
				return fast_table_[line] > 0;
			}
			return false;
		}

		bool has(const std::string& path, size_t line, lua_State* L, lua_Debug* ar) const
		{
			auto it = find(path);
			if (it == end())
			{
				return false;
			}
			auto& lines = it->second;
			auto lit = lines.find(line);
			if (lit == lines.end())
			{
				return false;
			}
			const std::string& condition = lit->second;
			if (condition.empty())
			{
				return true;
			}
			int n = lua_gettop(L);
			if (!evaluate(L, ar, ("return " + condition).c_str()))
			{
				lua_pop(L, 1);
				return false;
			}
			if (lua_type(L, -1) == LUA_TBOOLEAN	 && lua_toboolean(L, -1))
			{
				return true;
			}
			lua_settop(L, n);
			return false;
		}

	private:
		hybridarray<size_t, 1024> fast_table_;
	};
}
