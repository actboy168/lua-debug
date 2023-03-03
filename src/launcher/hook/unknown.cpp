#include <hook/vmhook_template.hpp>

namespace luadebug::autoattach {
    void vmhooker::call_origin_hook(lua::state L, lua::debug ar) {
        if (origin_lua_hook)
            origin_lua_hook(L, ar);
    }

    void vmhooker::reset_lua_hook(lua::state L) {
        if (origin_lua_hook)
            lua::call<lua_sethook>(L, origin_lua_hook, origin_hookmask, origin_hookcount);
    }

    void vmhooker::call_lua_sethook(lua::state L, lua::hook fn) {
		origin_lua_hook = lua::call<lua_gethook>(L);
		origin_hookmask = lua::call<lua_gethookmask>(L);
		origin_hookcount = lua::call<lua_gethookcount>(L);
        lua::call<lua_sethook>(L, fn, 0 | 1 | 2, 0);
    }
}