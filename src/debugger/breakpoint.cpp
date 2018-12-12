#include <debugger/breakpoint.h>
#include <debugger/evaluate.h>
#include <debugger/impl.h>
#include <debugger/lua.h>
#include <regex>

namespace vscode
{
	std::string lua_tostr(lua_State* L, int idx);

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
		std::string res = lua_tostr(L, -nresult);
		lua_pop(L, nresult);
		return res;
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

	bp_breakpoint::bp_breakpoint(size_t id, rapidjson::Value const& info)
		: id(id)
		, line(0)
		, verified(false)
		, cond()
		, hitcond()
		, log()
		, hit(0)
	{
		line = info["line"].GetUint();
		update(info);
	}

	void bp_breakpoint::update(rapidjson::Value const& info)
	{
		if (info.HasMember("condition")) {
			cond = info["condition"].Get<std::string>();
		}
		else {
			cond.clear();
		}
		if (info.HasMember("hitCondition")) {
			hitcond = info["hitCondition"].Get<std::string>();
		}
		else {
			hitcond.clear();
		}
		if (info.HasMember("logMessage")) {
			log = info["logMessage"].Get<std::string>();
		}
		else {
			log.clear();
		}
	}

	bool bp_breakpoint::verify(bp_source& src, debugger_impl* dbg)
	{
		if (verified) return true;
		for (size_t i = line; i < src.defined.size(); ++i) {
			switch (src.defined[i]) {
			case eLine::unknown:
				return false;
			case eLine::defined:
				if (src.verified.find(i) != src.verified.end()) {
					return false;
				}
				verified = true;
				line = i;
				if (dbg) {
					dbg->event_breakpoint("changed", this);
				}
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
		res("id").Uint64(id);
		res("verified").Bool(verified);
		res("line").Uint64(line);
		//if (cond.empty()) { res("condition").String(cond); }
		//if (hitcond.empty()) { res("hitCondition").String(hitcond); }
		//if (log.empty()) { res("logMessage").String(log); }
	}

	bool bp_breakpoint::run(debug& debug, debugger_impl& dbg)
	{
		if (debug.is_virtual()) {
			return true;
		}
		lua_State* L = debug.L();
		lua::Debug* ar = debug.value();
		if (!cond.empty() && !evaluate_isok(L, ar, cond)) {
			return false;
		}
		hit++;
		if (!hitcond.empty() && !evaluate_isok(L, ar, std::to_string(hit) + " " + hitcond)) {
			return false;
		}
		if (!log.empty()) {
			std::string res = evaluate_log(L, ar, log) + "\n";
			dbg.output("console", res.data(), res.size(), L, ar);
			return false;
		}
		return true;
	}

	bp_source::bp_source(debugger_impl& dbg_)
		: dbg(dbg_)
	{
	}

	bp_source::bp_source(bp_source&& s)
		: dbg(s.dbg)
		, defined(std::move(s.defined))
		, waitverfy(std::move(s.waitverfy))
		, verified(std::move(s.verified))
	{
	}

	bp_source::~bp_source()
	{
	}

	void bp_source::update(lua_State* L, lua::Debug* ar)
	{
		if (ar->what[0] == 'L') {
			while (defined.size() <= (size_t)ar->lastlinedefined) {
				defined.emplace_back(eLine::unknown);
			}
			for (int i = ar->linedefined; i <= ar->lastlinedefined; ++i) {
				if (defined[i] != eLine::defined) {
					defined[i] = eLine::unknown;
				}
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

		for (size_t i = 0; i < waitverfy.size();) {
			bp_breakpoint& bp = waitverfy[i];
			if (bp.verify(*this, &dbg)) {
				std::swap(waitverfy[i], waitverfy.back());
				verified.insert(std::make_pair(waitverfy.back().line, waitverfy.back()));
				waitverfy.pop_back();
			}
			else {
				++i;
			}
		}
	}

	bp_breakpoint& bp_source::add(size_t line, rapidjson::Value const& bpinfo, size_t& next_id)
	{
		for (auto& bp : waitverfy) {
			if (bp.line == line) {
				bp.update(bpinfo);
				return bp;
			}
		}
		auto it = verified.find(line);
		if (it != verified.end()) {
			it->second.update(bpinfo);
			return it->second;
		}
		bp_breakpoint bp(next_id++, bpinfo);
		if (bp.verify(*this)) {
			return verified.insert(std::make_pair(bp.line, bp)).first->second;
		}
		waitverfy.emplace_back(std::move(bp));
		return waitverfy.back();
	}

	bp_breakpoint* bp_source::get(size_t line)
	{
		auto it = verified.find(line);
		if (it == verified.end()) {
			return 0;
		}
		return &it->second;
	}

	void bp_source::clear(rapidjson::Value const& args)
	{
		std::set<size_t> lines;
		for (auto& m : args["breakpoints"].GetArray()) {
			lines.insert(m["line"].GetUint());
		}

		for (size_t i = 0; i < waitverfy.size();) {
			bp_breakpoint& bp = waitverfy[i];
			if (lines.find(bp.line) == lines.end()) {
				std::swap(waitverfy[i], waitverfy.back());
				waitverfy.pop_back();
			}
			else {
				++i;
			}
		}
		for (auto it = verified.begin(); it != verified.end();) {
			bp_breakpoint& bp = it->second;
			if (lines.find(bp.line) == lines.end()) {
				it = verified.erase(it);
			}
			else {
				++it;
			}
		}
	}

	bool bp_source::has_breakpoint()
	{
		return !verified.empty();
	}

	breakpointMgr::breakpointMgr(debugger_impl& dbg)
		: dbg_(dbg)
		, files_()
		, next_id_(0)
	{ }

	void breakpointMgr::clear()
	{
		files_.clear();
		memorys_.clear();
        functions_.clear();
	}

	bool breakpointMgr::has(bp_source* src, size_t line, debug& debug) const
	{
		bp_breakpoint* bp = src->get(line);
		if (!bp) {
			return false;
		}
		return bp->run(debug, dbg_);
	}

	bp_source& breakpointMgr::get_source(source& source)
	{
		if (source.ref) {
			auto it = memorys_.find(source.ref);
			if (it != memorys_.end()) {
				return it->second;
			}
			return memorys_.insert(std::make_pair(source.ref, bp_source(dbg_))).first->second;
		}
		else {
			auto it = files_.find(source.path);
			if (it != files_.end()) {
				return it->second;
			}
			return files_.insert(std::make_pair(source.path, bp_source(dbg_))).first->second;
		}
	}

	bp_source* breakpointMgr::get_function(debug& debug)
	{
		if (debug.is_virtual()) {
			source* s = dbg_.openVSource();
			bp_source* func = &get_source(*s);
			return func;
		}
		lua_State* L = debug.L();
		lua::Debug* ar = debug.value();
		if (!lua_getinfo(L, "f", (lua_Debug*)ar)) {
			return nullptr;
		}
		intptr_t f = (intptr_t)lua_getprotohash(L, -1);
		lua_pop(L, 1);
		bp_source* func = nullptr;
		if (functions_.get(f, func)) {
			return func;
		}
		if (lua_getinfo(L, "SL", (lua_Debug*)ar)) {
			source* s = dbg_.createSource(ar);
			if (s && s->valid) {
				func = &get_source(*s);
				func->update(L, ar);
			}
		}
		lua_pop(L, 1);
		functions_.put(f, func);
		return func;
	}

	void breakpointMgr::set_breakpoint(source& s, rapidjson::Value const& args, wprotocol& res)
	{
		bp_source& src = get_source(s);
		src.clear(args);
		for (auto _ : res("breakpoints").Array())
		{
			for (auto& m : args["breakpoints"].GetArray())
			{
				unsigned int line = m["line"].GetUint();
				bp_breakpoint& bp = src.add(line, m, next_id_);
				for (auto _ : res.Object())
				{
					bp.output(res);
				}
			}
		}
	}
}
