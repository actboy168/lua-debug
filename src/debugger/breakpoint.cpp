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

	source::source() {}

	source::source(rapidjson::Value const& info)
	{
		if (info.HasMember("path")) {
			path = info["path"].Get<std::string>();
			vaild = true;
		}
		else if (info.HasMember("name") && info.HasMember("sourceReference"))
		{
			ref = info["sourceReference"].Get<intptr_t>();
			vaild = true;
		}
	}

	void source::output(wprotocol& res)
	{
		if (!vaild) return;
		for (auto _ : res("source").Object())
		{
			if (ref) {
				res("sourceReference").Int64(ref);
			}
			else {
				res("path").String(path);
			}
		}
	}

	bp_breakpoint::bp_breakpoint(size_t id, rapidjson::Value const& info, int h)
		: id(id)
		, line(0)
		, verified(false)
		, verified_line(0)
		, cond()
		, hitcond()
		, log()
		, hit(h)
	{
		line = info["line"].GetUint();
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

	bool bp_breakpoint::verify(bp_source& src)
	{
		if (verified) return true;
		for (unsigned int i = line; i < src.defined.size(); ++i) {
			switch (src.defined[i]) {
			case eLine::unknown:
				return false;
			case eLine::defined:
				verified = true;
				verified_line = line;
				return true;
			case eLine::undef:
				break;
			default:
				return false;
			}
		}
		return false;
	}

	void bp_breakpoint::output(wprotocol& res)
	{
		res("id").Uint(id);
		res("verified").Bool(verified);
		res("line").Uint(verified ? verified_line : line);
	}

	bp_source::bp_source(source& s)
		: src(s)
	{ }

	void bp_source::update(lua_State* L, lua::Debug* ar, breakpoint* breakpoint)
	{
		while (defined.size() <= (size_t)ar->lastlinedefined) {
			defined.emplace_back(eLine::unknown);
		}
		for (int i = ar->linedefined; i <= ar->lastlinedefined; ++i) {
			if (defined[i] != eLine::defined) {
				defined[i] = eLine::undef;
			}
		}
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			int line = (int)lua_tointeger(L, -2);
			lua_pop(L, 1);
			while (defined.size() <= (size_t)line) {
				defined.emplace_back(eLine::unknown);
			}
			defined[line] = eLine::defined;
		}

		for (auto it : *this) {
			bp_breakpoint& bp = it.second;
			if (!bp.verified && bp.verify(*this)) {
				breakpoint->event_breakpoint("changed", this, &bp);
			}
		}
	}

	bp_function::bp_function(lua_State* L, lua::Debug* ar, breakpoint* breakpoint)
		: src(nullptr)
	{
		if (!lua_getinfo(L, "SL", (lua_Debug*)ar)) {
			return;
		}

		source s;
		if (ar->source[0] == '@' || ar->source[0] == '=') {
			if (breakpoint->get_pathconvert().get(ar->source, s.path)) {
				src = &breakpoint->get_source(s);
			}
		}
		else {
			s.ref = (intptr_t)ar->source;
			src = &breakpoint->get_source(s);
		}
		if (src) {
			s.vaild = true;
			src->update(L, ar, breakpoint);
		}
		lua_pop(L, 1);
	}

	breakpoint::breakpoint(debugger_impl* dbg)
		: dbg_(dbg)
		, files_()
		, next_id_(0)
	{ }

	void breakpoint::clear()
	{
		files_.clear();
		memorys_.clear();
	}

	void breakpoint::clear(source& source)
	{
		if (source.ref) {
			auto it = memorys_.find(source.ref);
			if (it != memorys_.end()) {
				it->second.clear();
				return;
			}
		}
		else {
			auto it = files_.find(source.path);
			if (it != files_.end()) {
				it->second.clear();
				return;
			}
		}
	}

	bp_breakpoint& breakpoint::add_breakpoint(bp_source& src, size_t line, rapidjson::Value const& bpinfo)
	{
		auto it = src.find(line);
		if (it != src.end()) {
			it->second = bp_breakpoint(next_id_++, bpinfo, it->second.hit);
			return it->second;
		}
		else {
			return src.insert(std::make_pair(line, bp_breakpoint(next_id_++, bpinfo, 0))).first->second;
		}
	}

	bp_breakpoint& breakpoint::add(source& source, size_t line, rapidjson::Value const& bpinfo)
	{
		bp_source& src = get_source(source); 
		bp_breakpoint& bp = add_breakpoint(src, line, bpinfo);
		bp.verify(src);
		return bp;
	}

	bool breakpoint::has(bp_source* src, size_t line, lua_State* L, lua::Debug* ar) const
	{
		auto it = src->find(line);
		if (it == src->end())
		{
			return false;
		}
		bp_breakpoint& bp = it->second;
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

	bp_source& breakpoint::get_source(source& source)
	{
		if (source.ref) {
			auto it = memorys_.find(source.ref);
			if (it != memorys_.end()) {
				return it->second;
			}
			return memorys_.insert(std::make_pair(source.ref, bp_source(source))).first->second;
		}
		else {
			auto it = files_.find(source.path);
			if (it != files_.end()) {
				return it->second;
			}
			return files_.insert(std::make_pair(source.path, bp_source(source))).first->second;
		}
	}

	bp_function* breakpoint::get_function(lua_State* L, lua::Debug* ar)
	{
		if (!lua_getinfo(L, "f", (lua_Debug*)ar)) {
			return nullptr;
		}
		intptr_t f = (intptr_t)lua_topointer(L, -1);
		lua_pop(L, 1);
		auto func = functions_.get(f);
		if (!func) {
			func = new bp_function(L, ar, this);
			functions_.put(f, func);
		}
		return func->src ? func : nullptr;
	}

	pathconvert& breakpoint::get_pathconvert()
	{
		return dbg_->get_pathconvert();
	}

	void breakpoint::event_breakpoint(const char* reason, bp_source* src, bp_breakpoint* bp)
	{
		dbg_->event_breakpoint(reason, src, bp);
	}
}
