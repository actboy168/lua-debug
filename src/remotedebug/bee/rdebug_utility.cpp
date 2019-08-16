#define RLUA_REPLACE
#include "../rlua.h"
#if defined(_WIN32)
#include <binding/lua_unicode.cpp>
#else
#include <bee/lua/binding.h>
#endif
#include <bee/filesystem.h>

namespace bee::lua_filesystem {
    static int absolute(lua_State* L) {
#if defined(_WIN32)
#define FS_ABSOLUTE(path) fs::absolute(path)
#else
#define FS_ABSOLUTE(path) fs::absolute(path).lexically_normal()
#endif
        LUA_TRY;
        auto res = FS_ABSOLUTE(fs::path(lua::tostring<fs::path::string_type>(L, 1))).generic_u8string();
        lua_pushlstring(L, res.data(), res.size());
        return 1;
        LUA_TRY_END;
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
        {"fs_absolute", bee::lua_filesystem::absolute},
        {NULL, NULL}};
    luaL_setfuncs(L, lib, 0);
    return 1;
}
