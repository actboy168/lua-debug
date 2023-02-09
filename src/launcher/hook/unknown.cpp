#include "vmhook_template.hpp"
#include "../utility/string_helper.hpp"

namespace autoattach {
    void unknown_vmhook::attach_lua_Hook(lua_State *L, lua_Debug *ar) {
        if (origin_lua_hook) {
            lua_sethook(L, origin_lua_hook, origin_hookmask, origin_hookcount);
            origin_lua_hook(L, ar);
        }
    }

    void unknown_vmhook::call_lua_hook(lua_State *L, lua_Hook fn) {
        origin_lua_hook = lua_gethook(L);
        origin_hookmask = lua_gethookmask(L);
        origin_hookcount = lua_gethookcount(L);
        lua_sethook(L, fn, LUA_HOOKCALL | LUA_HOOKRET | LUA_HOOKLINE, 0);
    }

    bool unknown_vmhook::get_symbols(const std::unique_ptr <symbol_resolver::interface> &resolver) {
        SymbolResolverWithCheck(lua_sethook);
        SymbolResolverWithCheck(lua_gethook);
        SymbolResolverWithCheck(lua_gethookmask);
        SymbolResolverWithCheck(lua_gethookcount);
        return true;
    }

    vmhook_template &vmhook_template::get_this() {
        static vmhook_template _vmhook;
        return _vmhook;
    }

    std::vector <std::string_view> get_hook_entry_points(lua_version version) {
        const char *list = getenv("LUA_DEBUG_HOOK_ENTRY");
        if (list) {
            return strings::spilt_string(list, ',');
        }
        switch (version) {
            case lua_version::luajit:
                return {
                        "luajit.lj_dispatch_call",
                        "luajit.lj_dispatch_stitch",
                        "luajit.lj_vm_call",
                        "luajit.lj_vm_pcall",
                        "luajit.lj_vm_cpcall",
                        "luajit.lj_vm_resume",
                        "luajit.lj_meta_tget",
                        "luajit.lj_cf_ffi_abi"
                };
            case lua_version::lua51:
                return {
                        "lua51.luaD_precall",
                        "lua51.luaV_gettable",
                        "lua51.luaV_settable"
                };
            case lua_version::lua54:
                return {
                        "lua54.luaD_poscall",
                        "lua54.luaH_getint",
                        "lua54.luaH_getshortstr",
                        "lua54.luaH_getstr",
                        "lua54.luaH_get"
                };
            default:
                return {
                        "lua_settop",
                        "luaL_openlibs",
                        "lua_newthread",
                        "lua_close",
                        "lua_type",
                        "lua_pushnil",
                        "lua_pushnumber",
                        "lua_pushlstring",
                        "lua_pushboolean"
                };
        }
    }

    vmhook *create_vmhook(lua_version version) {
        vmhook_template &hk = vmhook_template::get_this();
        hk.wather_points.clear();
        auto entry = get_hook_entry_points(version);
        for (auto &&e: entry) {
            hk.wather_points.emplace_back(watch_point{std::string{e}, 0});
        }
        return &hk;
    }
}