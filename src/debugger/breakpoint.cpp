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

	source::source(lua::Debug* ar, pathconvert& pathconvert)
	{
		if (ar->source[0] == '@' || ar->source[0] == '=') {
			if (pathconvert.get(ar->source, path)) {
				vaild = true;
			}
		}
		else {
			ref = (intptr_t)ar->source;
			vaild = true;
		}
	}

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
				res("name").String("<Memory>");
				res("sourceReference").Int64(ref);
			}
			else {
				res("name").String(path::filename(path));
				res("path").String(path);
			}
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
					dbg->event_breakpoint("changed", &src, this);
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

	bool bp_breakpoint::run(lua_State* L, lua::Debug* ar, debugger_impl* dbg)
	{
		if (!cond.empty() && !evaluate_isok(L, ar, cond)) {
			return false;
		}
		hit++;
		if (!hitcond.empty() && !evaluate_isok(L, ar, std::to_string(hit) + " " + hitcond)) {
			return false;
		}
		if (!log.empty()) {
			std::string res = evaluate_log(L, ar, log) + "\n";
			dbg->output("stdout", res.data(), res.size(), L, ar);
			return false;
		}
		return true;
	}

	bp_source::bp_source(source& s)
		: src(s)
	{ }

	void bp_source::update(lua_State* L, lua::Debug* ar, debugger_impl* dbg)
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
			if (bp.verify(*this, dbg)) {
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
		for (auto& it = verified.begin(); it != verified.end();) {
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

	bp_function::bp_function(lua_State* L, lua::Debug* ar, debugger_impl* dbg, breakpoint* breakpoint)
		: src(nullptr)
	{
		if (!lua_getinfo(L, "SL", (lua_Debug*)ar)) {
			return;
		}
		source s(ar, dbg->get_pathconvert());
		if (s.vaild) {
			src = &breakpoint->get_source(s);
			src->update(L, ar, dbg);
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

	bool breakpoint::has(bp_source* src, size_t line, lua_State* L, lua::Debug* ar) const
	{
		bp_breakpoint* bp = src->get(line);
		if (!bp) {
			return false;
		}
		return bp->run(L, ar, dbg_);
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
		intptr_t f = (intptr_t)lua_getproto(L, -1);
		lua_pop(L, 1);
		auto func = functions_.get(f);
		if (!func) {
			func = new bp_function(L, ar, dbg_, this);
			functions_.put(f, func);
		}
		return func->src ? func : nullptr;
	}

	void breakpoint::set_breakpoint(source& s, rapidjson::Value const& args, wprotocol& res)
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
