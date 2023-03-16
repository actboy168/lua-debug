#include <config/config.h>
#include <autoattach/lua_module.h>
#include <resolver/lua_signature.h>
#include <util/log.h>

#include <fstream>
#include <iostream>
#include <regex>

#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/format.h>
#include <bee/utility/path_helper.h>

#include <gumpp.hpp>

#include <nlohmann/json.hpp>

namespace luadebug::autoattach {
    static lua_version lua_version_from_string [[maybe_unused]] (const std::string_view& v) {
        if (v == "luajit")
            return lua_version::luajit;
        if (v == "lua51")
            return lua_version::lua51;
        if (v == "lua52")
            return lua_version::lua52;
        if (v == "lua53")
            return lua_version::lua53;
        if (v == "lua54")
            return lua_version::lua54;
        return lua_version::unknown;
    }

    Config config;

    lua_version Config::get_lua_version() const {
        std::string key = "lua_version";

        auto it = values->find(key);
        if (!values || it == values->end()) {
            return lua_version::unknown;
        }

        return lua_version_from_string(it->get<std::string>());
    }

    std::optional<signture> Config::get_lua_signature(const std::string& key) const {
        auto it = values->find(key);
        if (it == values->end()) {
            return std::nullopt;
        }
        const auto& value = it->get<std::string>();
        if (value.empty())
            return std::nullopt;
        // value string match regex
        std::regex re(R"re((?:\[([-+]?\d+)(?::([-+]?\d+)\])?)?([ \da-fA-F]+)([-+]\d+)?)re");
        std::smatch match;
        if (!std::regex_match(value, match, re))
            return std::nullopt;

        signture res = {};
        if (match[1].matched)
            res.start_offset = std::atoi(match[1].str().c_str());

        if (match[2].matched)
            res.end_offset = std::atoi(match[2].str().c_str());

        res.pattern = match[3].str();

        if (match[4].matched)
            res.pattern_offset = std::atoi(match[4].str().c_str());

        return res;
    }

    std::string Config::get_lua_module() const {
        std::string key = "lua_module";

        auto it = values->find(key);
        if (it == values->end()) {
            return {};
        }
        const auto& value = it->get<std::string>();
        if (!fs::exists(value))
            return {};

        return value;
    }

    bool Config::is_remotedebug_by_signature() const {
        std::string key = "remotedebug_by_signature";
        return values->find(key) != values->end();
    }

    bool Config::init_from_file() {
        config.values = std::make_unique<nlohmann::json>();
        auto dllpath = bee::path_helper::dll_path();
        if (!dllpath) {
            return false;
        }

        auto filename = std::format("{}/tmp/pid_{}_config", dllpath.value().parent_path().parent_path().generic_string(), Gum::Process::get_id());

        std::ifstream s(filename, s.in);
        if (!s.is_open())
            return false;

        s >> *config.values;
        return true;
    }

}// namespace luadebug::autoattach