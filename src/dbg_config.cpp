#include "dbg_config.h"
#include <rapidjson/error/en.h>
#include <base/util/format.h>

namespace vscode {
	bool config::init(int level, const rapidjson::Value& cfg)
	{
		if (level < 0 || level >= (int)doc.max_size()) {
			return false;
		}
		val[level] = rapidjson::Value(cfg, doc[level].GetAllocator(), true);
		return true;
	}

	bool config::init(int level, const std::string& cfg)
	{
		if (level < 0 || level >= (int)doc.max_size()) {
			return false;
		}
		if (doc[level].Parse(cfg.data(), cfg.size()).HasParseError()) {
			return false;
		}
		val[level] = doc[level].Move();
		return true;
	}

	bool config::init(int level, const std::string& cfg, std::string& err)
	{
		if (level < 0 || level >= (int)doc.max_size()) {
			return false;
		}
		if (doc[level].Parse(cfg.data(), cfg.size()).HasParseError()) {
			err = base::format("Error(offset %u): %s\n", static_cast<unsigned>(doc[level].GetErrorOffset()), rapidjson::GetParseError_En(doc[level].GetParseError()));
			return false;
		}
		val[level] = doc[level].Move();
		return true;
	}

	static bool isBool(rapidjson::Type type) {
		return (type == rapidjson::kTrueType || type == rapidjson::kFalseType);
	}

	rapidjson::Value const& config::get(const std::string& key, rapidjson::Type type) const
	{
		if (isBool(type)) {
			for (rapidjson::Value const& cfg : val) {
				if (cfg.IsObject() && cfg.HasMember(key) && isBool(cfg[key].GetType())) {
					return cfg[key];
				}
			}
		}
		else {
			for (rapidjson::Value const& cfg : val) {
				if (cfg.IsObject() && cfg.HasMember(key) && cfg[key].GetType() == type) {
					return cfg[key];
				}
			}
		}
		static rapidjson::Value s_dummy;
		s_dummy = rapidjson::Value(type);
		return s_dummy;
	}
}
