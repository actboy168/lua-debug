#include <autoattach/lua_module.h>
#include <bee/nonstd/format.h>
#include <hook/create_watchdog.h>
#include <hook/watchdog.h>
#include <util/log.h>

#include <charconv>
#include <gumpp.hpp>

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

    static const char* lua_version_to_string(lua_version v) {
        switch (v) {
        case lua_version::lua51:
            return "lua51";
        case lua_version::lua52:
            return "lua52";
        case lua_version::lua53:
            return "lua53";
        case lua_version::lua54:
            return "lua54";
        case lua_version::luajit:
            return "luajit";
        default:
            return "unknown";
        }
    }

    static bool in_module(const lua_module& m, void* addr) {
        return addr > m.memory_address && addr <= (void*)((intptr_t)m.memory_address + m.memory_size);
    }

    static lua_version get_lua_version(const lua_module& lm) {
        /*
            luaJIT_version_2_1_0_beta3
            luaJIT_version_2_1_0_beta2
            luaJIT_version_2_1_0_beta1
            luaJIT_version_2_1_0_alpha
        */
        for (void* addr : Gum::SymbolUtil::find_matching_functions("luaJIT_version_2_1_0*", true)) {
            if (in_module(lm, addr))
                return lua_version::luajit;
        }
        auto m = Gum::Process::find_module_by_name(lm.path.c_str());

        auto p = m->find_symbol_by_name("lua_ident");
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

    bool lua_module::initialize() {
        resolver.module_name = path;
        auto error_msg       = lua::initialize(resolver);
        if (error_msg) {
            log::fatal("lua initialize failed, can't find {}", error_msg);
            return false;
        }
        version = get_lua_version(*this);
        log::info("current lua version: {}", lua_version_to_string(version));

        watchdog = create_watchdog(mode, version, resolver);
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
