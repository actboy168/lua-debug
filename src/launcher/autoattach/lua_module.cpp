#include <autoattach/ctx.h>
#include <autoattach/lua_module.h>
#include <bee/nonstd/format.h>
#include <bee/utility/path_helper.h>
#include <hook/create_watchdog.h>
#include <hook/watchdog.h>
#include <util/log.h>
#include <util/path.h>

#include <charconv>
#include <gumpp.hpp>

namespace luadebug::autoattach {
    static bool in_module(const lua_module& m, void* addr) {
        return addr >= m.memory_address && addr <= (void*)((intptr_t)m.memory_address + m.memory_size);
    }

    static lua_version get_lua_version(const lua_module& m) {
        /*
            luaJIT_version_2_1_0_beta3
            luaJIT_version_2_1_0_beta2
            luaJIT_version_2_1_0_beta1
            luaJIT_version_2_1_0_alpha
        */
        for (void* addr : Gum::SymbolUtil::find_matching_functions("luaJIT_version_2_1_0*", true)) {
            if (in_module(m, addr))
                return lua_version::luajit;
        }
        auto p = Gum::Process::module_find_symbol_by_name(m.path.c_str(), "lua_ident");
        ;
        const char* lua_ident = (const char*)p;
        if (!lua_ident)
            return lua_version::unknown;
        auto id = std::string_view(lua_ident);
        using namespace std::string_view_literals;
        constexpr auto key = "$Lua"sv;
        if (id.substr(0, key.length()) != key) {
            return lua_version::unknown;
        }
        if (id[key.length()] == ':') {
            //$Lua: Lua 5.1.*
            return lua_version::lua51;
        }
        id = id.substr("$LuaVersion: Lua 5."sv.length());
        int patch;
        auto res = std::from_chars(id.data(), id.data() + 3, patch, 10);
        if (res.ec != std::errc()) {
            return lua_version::unknown;
        }
        switch (patch) {
        case 1:
            return lua_version::lua51;
        case 2:
            return lua_version::lua52;
        case 3:
            return lua_version::lua53;
        case 4:
            return lua_version::lua54;
        default:
            return lua_version::unknown;
        }
    }

    bool load_luadebug_dll(lua_module* lm, lua_version version) {
        auto luadebug_path = get_luadebug_path(version);
        if (!luadebug_path)
            return false;
        auto path = (*luadebug_path).string();
        std::string error;
        if (!Gum::Process::module_load(path.c_str(), &error)) {
            log::fatal("load debugger [{}] failed: {}", path, error);
        }
        lm->debugger_path = path;
        return true;
    }

    bool lua_module::initialize() {
        resolver.module_name = path;
        auto error_msg       = lua::initialize(resolver);
        if (error_msg) {
            log::fatal("lua initialize failed, can't find {}", error_msg);
            return false;
        }
        if (version == lua_version::unknown) {
            version = get_lua_version(*this);
        }
        log::info("current lua version: {}", lua_version_to_string(version));

        watchdog = create_watchdog(*this);
        if (version != lua_version::unknown) {
            if (!load_luadebug_dll(this, version))
                return false;
        }
        if (!watchdog) {
            return false;
        }
        return true;
    }

    lua_module::~lua_module() {
        if (watchdog) {
            delete watchdog;
        }
    }
}
