#include <debugger/impl.h>
#include <debugger/source.h>
#include <debugger/breakpoint.h>

namespace vscode {

	source::source()
	{}

	void source::output(wprotocol& res)
	{
		if (!valid) return;
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

	sourceMgr::sourceMgr(debugger_impl& dbg)
		: dbg_(dbg)
	{ }

	source* sourceMgr::create(lua::Debug* ar) {
		if (ar->source[0] == '@' || ar->source[0] == '=') {
			std::string path;
			if (dbg_.path_convert(ar->source, path)) {
				return createByPath(path);
			}
		}
		else {
			return createByRef((int64_t)ar->source);
		}
		return nullptr;
	}

	source* sourceMgr::create(rapidjson::Value const& info) {
		if (info.HasMember("path")) {
			return createByPath(info["path"].Get<std::string>());
		}
		else if (info.HasMember("name") && info.HasMember("sourceReference")) {
			return createByRef(info["sourceReference"].GetInt64());
		}
		return nullptr;
	}

	source* sourceMgr::open(rapidjson::Value const& info) {
		if (info.HasMember("path")) {
			return openByPath(info["path"].Get<std::string>());
		}
		else if (info.HasMember("name") && info.HasMember("sourceReference")) {
			return openByRef(info["sourceReference"].GetInt64());
		}
		return nullptr;
	}

	source* sourceMgr::createByPath(const std::string& path) {
		auto it = poolPath_.find(path);
		if (it != poolPath_.end()) {
			return &(it->second);
		}
		auto res = poolPath_.insert(std::make_pair(path, source()));
		assert(res.second);
		source* s = &(res.first->second);
		s->valid = true;
		s->path = path;
		dbg_.event_loadedsource("new", s);
		return s;
	}

	source* sourceMgr::createByRef(int64_t ref) {
		auto it = poolRef_.find(ref);
		if (it != poolRef_.end()) {
			return &(it->second);
		}
		auto res = poolRef_.insert(std::make_pair(ref, source()));
		assert(res.second);
		source* s = &(res.first->second);
		s->valid = true;
		s->ref = ref;
		dbg_.event_loadedsource("new", s);
		return s;
	}

	source* sourceMgr::openByPath(const std::string& path) {
		auto it = poolPath_.find(path);
		if (it != poolPath_.end()) {
			return &(it->second);
		}
		return nullptr;
	}

	source* sourceMgr::openByRef(int64_t ref) {
		auto it = poolRef_.find(ref);
		if (it != poolRef_.end()) {
			return &(it->second);
		}
		return nullptr;
	}

	void sourceMgr::loadedSources(wprotocol& res)
	{
		for (auto _ : res("sources").Array()) {
			for (auto& p : poolPath_) {
				p.second.output(res);
			}
			for (auto& p : poolRef_) {
				p.second.output(res);
			}
		}
	}
}
