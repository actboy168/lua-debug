#include "dbg_config.h"
#include <rapidjson/error/en.h>
#include <base/util/format.h>

namespace vscode {
	bool config::init(int level, const rapidjson::Value& cfg)
	{
		if (level < 0 || level >= (int)max_size()) {
			return false;
		}
		rapidjson::Document doc;
		(*this)[level] = rapidjson::Value(cfg, doc.GetAllocator(), true);
		return true;
	}

	bool config::init(int level, const std::string& cfg)
	{
		if (level < 0 || level >= (int)max_size()) {
			return false;
		}
		rapidjson::Document doc;
		if (doc.Parse(cfg.data(), cfg.size()).HasParseError()) {
			return false;
		}
		(*this)[level] = doc.Move();
		return true;
	}

	bool config::init(int level, const std::string& cfg, std::string& err)
	{
		if (level < 0 || level >= (int)max_size()) {
			return false;
		}
		rapidjson::Document doc;
		if (doc.Parse(cfg.data(), cfg.size()).HasParseError()) {
			err = base::format("Error(offset %u): %s\n", static_cast<unsigned>(doc.GetErrorOffset()), rapidjson::GetParseError_En(doc.GetParseError()));
			return false;
		}
		(*this)[level] = doc.Move();
		return true;
	}

	rapidjson::Value const& config::get(const std::string& key, rapidjson::Type type) const
	{
		for (rapidjson::Value const& cfg : *this) {
			if (cfg.IsObject() && cfg.HasMember(key) && cfg[key].GetType() == type) {
				return cfg[key];
			}
		}
		static rapidjson::Value s_dummy;
		s_dummy = rapidjson::Value(type);
		return s_dummy;
	}
}
