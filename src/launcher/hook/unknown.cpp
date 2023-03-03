#include "vmhook_template.hpp"

namespace luadebug::autoattach {
    void vmhooker::call_origin_hook(lua::state L, lua::debug ar) {
        if (origin_lua_hook) {
            lua::call<lua_sethook>(L, origin_lua_hook, origin_hookmask, origin_hookcount);
            origin_lua_hook(L, ar);
        }
    }

    void vmhooker::call_lua_sethook(lua::state L, lua::hook fn) {
		origin_lua_hook = lua::call<lua_gethook>(L);
		origin_hookmask = lua::call<lua_gethookmask>(L);
		origin_hookcount = lua::call<lua_gethookcount>(L);
        lua::call<lua_sethook>(L, fn, 0 | 1 | 2, 0);
    }

    static std::vector <std::string_view> get_hook_entry_points(lua_version version) {
        switch (version) {
            case lua_version::luajit:
                return {
                        "lj_dispatch_ins",
                        "lj_dispatch_call",
                        "lj_vm_call",
                        "lj_vm_pcall",
                        "lj_vm_cpcall",
                        "lj_vm_resume",
                        "lj_meta_tget",
                };
            case lua_version::lua51:
                return {
                        "luaD_precall",
                        "luaV_gettable",
                        "luaV_settable"
                };
            case lua_version::lua54:
                return {
                        "luaD_poscall",
                        "luaH_finishset",
                        "luaV_finishget",
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

    static std::unique_ptr<vmhook_template> _vmhook;
    vmhook_template &vmhook_template::get_this() {
        return *_vmhook;
    }
    
    std::unique_ptr<vmhook_template> create_luajit_vmhook();
    vmhook *create_vmhook(lua_version version) {
        if (version == lua_version::luajit){
            _vmhook = create_luajit_vmhook();
        }else{
            _vmhook = std::make_unique<vmhook_template>();
        }
        
        vmhook_template &hk = vmhook_template::get_this();
        hk.wather_points.clear();
        auto entry = get_hook_entry_points(version);
        for (auto &&e: entry) {
            hk.wather_points.emplace_back(watch_point{std::string{e}, 0});
        }
        return &hk;
    }
}