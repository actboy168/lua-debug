#pragma once

struct lua_State;
struct luadbg_State;

namespace luadebug::debughost {
    luadbg_State* get_client(lua_State* L);
    lua_State* get(luadbg_State* L);
    void set(luadbg_State* L, lua_State* hostL);
}
