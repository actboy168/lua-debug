#include <debugger/breakpoint.h>
#include <debugger/evaluate.h>
#include <debugger/lua.h>

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

	void breakpoint::clear(const std::string& client_path)
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

	void breakpoint::add(const std::string& client_path, size_t line, const std::string& condition, const std::string& hitcondition)
	{
		return add(files_[client_path], line, condition, hitcondition);
	}

	void breakpoint::add(intptr_t source_ref, size_t line, const std::string& condition, const std::string& hitcondition)
	{
		return add(memorys_[source_ref], line, condition, hitcondition);
	}

	void breakpoint::add(bp_source& src, size_t line, const std::string& condition, const std::string& hitcondition)
	{
		auto it = src.find(line);
		if (it != src.end())
		{
			it->second = bp(condition, hitcondition, it->second.hit);
			return;
		}

		src.insert(std::make_pair(line, bp(condition, hitcondition, 0)));
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

	bool breakpoint::evaluate_isok(lua_State* L, lua::Debug *ar, const std::string& script) const
	{
		int nresult = 0;
		if (!evaluate(L, ar, ("return " + script).c_str(), nresult))
		{
			lua_pop(L, 1);
			return false;
		}
		if (nresult > 0 && lua_type(L, -nresult) == LUA_TBOOLEAN && lua_toboolean(L, -nresult))
		{
			lua_pop(L, nresult);
			return true;
		}
		lua_pop(L, nresult);
		return false;
	}

	bool breakpoint::has(bp_source* src, size_t line, lua_State* L, lua::Debug* ar) const
	{
		auto it = src->find(line);
		if (it == src->end())
		{
			return false;
		}
		bp& bp = it->second;
		if (!bp.condition.empty() && !evaluate_isok(L, ar, bp.condition))
		{
			return false;
		}
		bp.hit++;
		if (!bp.hitcondition.empty() && !evaluate_isok(L, ar, std::to_string(bp.hit) + " " + bp.hitcondition))
		{
			return false;
		}
		return true;
	}

	bp_source* breakpoint::get(const char* source, pathconvert& pathconvert)
	{
		if (source[0] == '@' || source[0] == '=')
		{
			std::string client_path;
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
