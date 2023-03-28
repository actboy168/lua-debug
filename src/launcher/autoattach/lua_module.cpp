#include <autoattach/lua_module.h>
#include <bee/nonstd/format.h>
#include <bee/utility/path_helper.h>
#include <config/config.h>
#include <hook/create_watchdog.h>
#include <resolver/lua_signature.h>
#include <util/log.h>

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
        // TODO: from signature
    }

    bool load_luadebug_dll(lua_version version) {
        auto luadebug_path = config::get_luadebug_path(version);
        if (!luadebug_path)
            return false;
        auto path = (*luadebug_path).string();
        std::string error;
        if (!Gum::Process::module_load(luadebug.c_str(), &error)) {
            log::fatal("load debugger [{}] failed: {}", luadebug, error);
        }
        Gum::Process::module_enumerate_import(luadebug.c_str(), [&](const Gum::ImportDetails& details) -> bool {
            if (std::string_view(details.name).find_first_of("lua") != 0) {
                return true;
            }
            if (auto address = (void*)resolver.find(details.name)) {
                *details.slot = address;
                log::info("find signature {} to {}", details.name, address);
            }
            return true;
        });
        return true;
    }

    bool lua_module::initialize(fn_attach attach_lua_vm) {
        version = get_lua_version(*this);
        log::info("current lua version: {}", lua_version_to_string(version));

        resolver = std::make_unique<lua_resolver>(path);
        if (version != lua_version::unknown && config.is_signature_mode()) {
            // 尝试从自带的库里加载函数
            auto dir = get_luadebug_dir(version);
            if (dir) {
                auto dllpath =
#ifdef _WIN32
                    (*dir / lua_version_to_string(version) + std::string(EXT)).string();
#else
                    (*dir / "lua" EXT).string();
#endif
                std::string error;
                if (!Gum::Process::module_load(dllpath.c_str(), &error)) {
                    log::info("load lua module {}, failed: {}", dllpath, error);
                }
                else {
                    auto signature_resolver_ptr      = std::make_unique<signature_resolver>();
                    signature_resolver_ptr->resolver = std::move(resolver);
                    signature_resolver_ptr->reserve_resolver =
                        std::make_unique<lua_resolver>(dllpath);
                    resolver = std::move(signature_resolver_ptr);
                }
            }
        }

        auto error_msg = lua::initialize(*resolver);
        if (error_msg) {
            log::fatal("lua initialize failed, can't find {}", error_msg);
            return false;
        }
        version = config.version;
        if (version == lua_version::unknown) {
            version = get_lua_version(*this);
        }
        log::info("current lua version: {}", lua_version_to_string(version));

        if (version != lua_version::unknown) {
            if (!load_luadebug_dll(version, *resolver))
                return false;
        }

        watchdog = create_watchdog(attach_lua_vm, version, *resolver);
        if (!watchdog) {
            // TODO: more errmsg
            log::fatal("watchdog initialize failed");
            return false;
        }
        return true;
    }
}
