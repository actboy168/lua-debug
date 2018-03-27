#pragma once

#include <rapidjson/document.h>
#include <array>

namespace vscode {
	struct config 
	{
		bool                    init(int level, const rapidjson::Value& cfg);
		bool                    init(int level, const std::string& cfg);
		bool                    init(int level, const std::string& cfg, std::string& err);
		rapidjson::Value const& get (const std::string& key, rapidjson::Type type) const;

		std::array<rapidjson::Value, 3>    val;
		std::array<rapidjson::Document, 3> doc;
	};
}
