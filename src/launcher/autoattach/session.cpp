#include <autoattach/lua_module.h>
#include <autoattach/session.h>
#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/format.h>
#include <bee/utility/path_helper.h>
#include <util/log.h>
#include <util/path.h>

#include <fstream>
#include <gumpp.hpp>
#include <nlohmann/json.hpp>
#include <string_view>

using namespace std::string_view_literals;

namespace luadebug::autoattach {
    static lua_version get_lua_version(nlohmann::json& values) {
        const auto key = "version"sv;
        auto it        = values.find(key);
        if (it == values.end()) {
            return lua_version::unknown;
        }
        return lua_version_from_string(it->get<std::string>());
    }

    static std::string get_lua_module(nlohmann::json& values) {
        const auto key = "module"sv;

        auto it = values.find(key);
        if (it == values.end()) {
            return {};
        }
        auto value = it->get<std::string>();
        if (!fs::exists(value))
            return value;

        do {
            if (!fs::is_symlink(value))
                return value;
            auto link = fs::read_symlink(value);
            if (link.is_absolute())
                value = link.string();
            else
                value = (fs::path(value).parent_path() / link).lexically_normal().string();
        } while (true);
    }

    std::optional<session> init_session(work_mode mode) {
        session session;
        nlohmann::json values;

        auto tmp = get_tmp_dir();
        if (!tmp)
            return std::nullopt;
        auto filename = ((*tmp) / std::format("ipc_{}_config", Gum::Process::get_id())).string();

        std::ifstream s(filename, s.binary);
        if (!s.is_open())
            return std::nullopt;
        try {
            s >> values;
        } catch (const nlohmann::json::exception& e) {
            log::info("init_from_file error: {}", e.what());
        }

        session.mode       = mode;
        session.version    = get_lua_version(values);
        session.lua_module = get_lua_module(values);

        return session;
    }
}  // namespace luadebug::autoattach
