#include <debugger/breakpoint.h>
#include <debugger/evaluate.h>
#include <debugger/impl.h>
#include <debugger/lua.h>
#include <regex>

namespace vscode
{
	static bool evaluate_isok(lua_State* L, lua::Debug *ar, const std::string& script)
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

	static std::string evaluate_getstr(lua_State* L, lua::Debug *ar, const std::string& script)
	{
		int nresult = 0;
		if (!evaluate(L, ar, ("return tostring(" + script + ")").c_str(), nresult))
		{
			lua_pop(L, 1);
			return "";
		}
		if (nresult <= 0)
		{
			return "";
		}
		std::string res;
		size_t len = 0;
		const char* str = lua_tolstring(L, -nresult, &len);
		lua_pop(L, nresult);
		return std::string(str, len);
	}

	static std::string evaluate_log(lua_State* L, lua::Debug *ar, const std::string& log)
	{
		try {
			std::string res;
			std::regex re(R"(\{[^\}]*\})");
			std::smatch m;
			auto it = log.begin();
			for (; std::regex_search(it, log.end(), m, re); it = m[0].second) {
				res += std::string(it, m[0].first);
				res += evaluate_getstr(L, ar, std::string(m[0].first + 1, m[0].second - 1));
			}
			res += std::string(it, log.end());
			return res;
		}
		catch (std::exception& e) {
			return e.what();
		}
	}

	bp::bp(rapidjson::Value const& info, int h)
		: cond()
		, hitcond()
		, log()
		, hit(h)
	{
		if (info.HasMember("condition")) {
			cond = info["condition"].Get<std::string>();
		}
		if (info.HasMember("hitCondition")) {
			hitcond = info["hitCondition"].Get<std::string>();
		}
		if (info.HasMember("logMessage")) {
			log = info["logMessage"].Get<std::string>() + "\n";
		}
	}

	bp_function::bp_function(lua_State* L, lua::Debug* ar, breakpoint* breakpoint)
		: vaild(false)
		, clientpath()
		, bp(nullptr)
	{
		if (!lua_getinfo(L, "S", (lua_Debug*)ar)) {
			return;
		}

		if (ar->source[0] == '@' || ar->source[0] == '=') {
			if (breakpoint->get_pathconvert().get(ar->source, clientpath))
			{
				bp = breakpoint->get_file_bp(clientpath);
				vaild = true;
			}
		}
		else {
			bp = breakpoint->get_memory_bp((intptr_t)ar->source);
			vaild = true;
		}
	}

	breakpoint::breakpoint(debugger_impl* dbg)
		: dbg_(dbg)
		, files_()
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

	void breakpoint::add(const std::string& client_path, size_t line, rapidjson::Value const& bp)
	{
		return add(files_[client_path], line, bp);
	}

	void breakpoint::add(intptr_t source_ref, size_t line, rapidjson::Value const& bp)
	{
		return add(memorys_[source_ref], line, bp);
	}

	void breakpoint::add(bp_source& src, size_t line, rapidjson::Value const& bpinfo)
	{
		auto it = src.find(line);
		if (it != src.end())
		{
			it->second = bp(bpinfo, it->second.hit);
			return;
		}

		src.insert(std::make_pair(line, bp(bpinfo, 0)));
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

	bool breakpoint::has(bp_source* src, size_t line, lua_State* L, lua::Debug* ar) const
	{
		auto it = src->find(line);
		if (it == src->end())
		{
			return false;
		}
		bp& bp = it->second;
		if (!bp.cond.empty() && !evaluate_isok(L, ar, bp.cond))
		{
			return false;
		}
		bp.hit++;
		if (!bp.hitcond.empty() && !evaluate_isok(L, ar, std::to_string(bp.hit) + " " + bp.hitcond))
		{
			return false;
		}
		if (!bp.log.empty())
		{
			std::string res = evaluate_log(L, ar, bp.log);
			dbg_->output("stdout", res.data(), res.size(), L, ar);
			return false;
		}
		return true;
	}

	bp_source* breakpoint::get_file_bp(const std::string& clientpath)
	{
		auto it = files_.find(clientpath);
		if (it != files_.end())
		{
			return &it->second;
		}
		return nullptr;
	}

	bp_source* breakpoint::get_memory_bp(intptr_t sourceref)
	{
		auto it = memorys_.find(sourceref);
		if (it != memorys_.end())
		{
			return &it->second;
		}
		return nullptr;
	}

	bp_function* breakpoint::get(lua_State* L, lua::Debug* ar)
	{
		if (!lua_getinfo(L, "f", (lua_Debug*)ar)) {
			return nullptr;
		}
		intptr_t f = (intptr_t)lua_topointer(L, -1);
		lua_pop(L, 1);
		auto it = functions_.find(f);
		if (it != functions_.end()) {
			return &(it->second);
		}
		return &(functions_.insert(std::make_pair(f, bp_function(L, ar, this))).first->second);
	}

	pathconvert& breakpoint::get_pathconvert()
	{
		return dbg_->get_pathconvert();
	}
}
