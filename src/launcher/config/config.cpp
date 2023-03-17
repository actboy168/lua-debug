#include <autoattach/lua_module.h>
#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/format.h>
#include <bee/utility/path_helper.h>
#include <config/config.h>
#include <util/log.h>

#include <fstream>
#include <gumpp.hpp>
#include <nlohmann/json.hpp>
#include <string_view>

using namespace std::string_view_literals;
namespace luadebug::autoattach {
    static lua_version lua_version_from_string [[maybe_unused]] (const std::string_view& v) {
        if (v == "jit" || v == "luajit")
            return lua_version::luajit;
        else if (v == "5.1" || v == "lua51")
            return lua_version::lua51;
        else if (v == "5.2" || v == "lua52")
            return lua_version::lua52;
        else if (v == "5.3" || v == "lua53")
            return lua_version::lua53;
        else if (v == "5.4" || v == "lua54")
            return lua_version::lua54;
        return lua_version::unknown;
    }

    Config config;

    lua_version Config::get_lua_version() const {
        const auto key = "version"sv;

        auto it = values->find(key);
        if (!values || it == values->end()) {
            return lua_version::unknown;
        }

        return lua_version_from_string(it->get<std::string>());
    }

    std::string Config::get_lua_module() const {
        const auto key = "module"sv;

        auto it = values->find(key);
        if (it == values->end()) {
            return {};
        }
        const auto& value = it->get<std::string>();
        if (!fs::exists(value))
            return {};

        return value;
    }

    bool Config::init_from_file() {
        config.values = std::make_unique<nlohmann::json>();
        auto dllpath  = bee::path_helper::dll_path();
        if (!dllpath) {
            return false;
        }

        auto filename = std::format("{}/tmp/pid_{}_config", dllpath.value().parent_path().parent_path().generic_string(), Gum::Process::get_id());

        std::ifstream s(filename, s.in);
        if (!s.is_open())
            return false;
        try {
            s >> *config.values;
        } catch (const nlohmann::json::exception& e) {
            log::info("init_from_file error: {}", e.what());
        }

        return true;
    }

}  // namespace luadebug::autoattach