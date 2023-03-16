#include <config/config.h>
#include <autoattach/lua_module.h>
#include <resolver/lua_signature.h>
#include <util/log.h>

#include <fstream>
#include <iostream>
#include <string_view>

#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/format.h>
#include <bee/utility/path_helper.h>

#include <gumpp.hpp>

#include <nlohmann/json.hpp>

using namespace std::string_view_literals;
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
        const auto key = "lua_version"sv;

        auto it = values->find(key);
        if (!values || it == values->end()) {
            return lua_version::unknown;
        }

        return lua_version_from_string(it->get<std::string>());
    }

    std::optional<signture> Config::get_lua_signature(const std::string& key) const {
        const auto signture_key = "signture"sv;
        auto it = values->find(signture_key);
        if (it == values->end()) {
            return std::nullopt;
        }

        //split key by .
        std::vector<std::string_view> keys;
        std::string_view key_view = key;
        while (!key_view.empty()) {
            auto pos = key_view.find('.');
            if (pos == std::string_view::npos) {
                keys.push_back(key_view);
                break;
            }
            keys.push_back(key_view.substr(0, pos));
            key_view = key_view.substr(pos + 1);
        }

        nlohmann::json& json = *it;
        for (const auto& key: keys) {
            json = json[key];
        }

        // searilize json to signture
        signture res = {};
        json["name"].get_to(res.name);
        json["start_offset"].get_to(res.start_offset);
        json["end_offset"].get_to(res.end_offset);
        json["pattern"].get_to(res.pattern);
        json["pattern_offset"].get_to(res.pattern_offset);
        json["hit_offset"].get_to(res.hit_offset);
        return res;
    }

    std::string Config::get_lua_module() const {
        const auto key = "lua_module"sv;

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
        const auto key = "remotedebug_by_signature"sv;
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

} // namespace luadebug::autoattach