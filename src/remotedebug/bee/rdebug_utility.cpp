#define RLUA_REPLACE
#include "../rlua.h"
#if defined(_WIN32)
#include <binding/lua_unicode.cpp>
#else
#include <bee/lua/binding.h>
#endif
#include <bee/filesystem.h>
#include <bee/platform.h>

namespace rdebug_utility {
    static int fs_absolute(lua_State* L) {
#if defined(_WIN32)
#define FS_ABSOLUTE(path) fs::absolute(path)
#else
#define FS_ABSOLUTE(path) fs::absolute(path).lexically_normal()
#endif
        try {
            auto res = FS_ABSOLUTE(fs::path(bee::lua::tostring<fs::path::string_type>(L, 1))).generic_u8string();
            lua_pushlstring(L, res.data(), res.size());
            return 1;
        } catch (const std::exception& e) {
            return bee::lua::push_error(L, e);
        }
    }
    static int platform_os(lua_State* L) {
        lua_pushstring(L, BEE_OS_NAME);
        return 1;
    }
}

RLUA_FUNC
int luaopen_remotedebug_utility(lua_State* L) {
#if defined(_WIN32)
    luaopen_bee_unicode(L);
#else
    lua_newtable(L);
#endif
    luaL_Reg lib[] = {
        {"fs_absolute", rdebug_utility::fs_absolute},
        {"platform_os", rdebug_utility::platform_os},
        {NULL, NULL}};
    luaL_setfuncs(L, lib, 0);
    return 1;
}
