#pragma once

#define lua_State luaX_State
#define lua_KContext luaX_KContext
#define lua_CFunction luaX_CFunction
#define lua_KFunction luaX_KFunction
#define luaL_Buffer luaXL_Buffer
#define luaL_Reg luaXL_Reg
#define luaL_Stream luaXL_Stream

#define lua_Integer luaX_Integer
#define lua_Unsigned luaX_Unsigned
#define lua_Number luaX_Number

#define luaL_newstate luaXL_newstate
#define luaL_openlibs luaXL_openlibs
#define luaL_loadstring luaXL_loadstring
#define luaL_loadbufferx luaXL_loadbufferx
#define luaL_setfuncs luaXL_setfuncs
#define luaL_checktype luaXL_checktype
#define luaL_checkoption luaXL_checkoption
#define luaL_checkudata luaXL_checkudata
#define luaL_newmetatable luaXL_newmetatable
#define luaL_setmetatable luaXL_setmetatable
#define luaL_callmeta luaXL_callmeta
#define luaL_checklstring luaXL_checklstring
#define luaL_checknumber luaXL_checknumber
#define luaL_optnumber luaXL_optnumber
#define luaL_checkinteger luaXL_checkinteger
#define luaL_optinteger luaXL_optinteger
#define luaL_len luaXL_len

#define lua_close luaX_close
#define lua_pcallk luaX_pcallk
#define lua_callk luaX_callk
#define lua_type luaX_type
#define lua_typename luaX_typename
#define lua_error luaX_error
#define luaL_error luaXL_error
#define luaL_traceback luaXL_traceback

#define lua_newuserdatauv luaX_newuserdatauv
#define lua_getupvalue luaX_getupvalue

#define lua_pushboolean luaX_pushboolean
#define lua_pushinteger luaX_pushinteger
#define lua_pushnumber luaX_pushnumber
#define lua_pushlightuserdata luaX_pushlightuserdata
#define lua_pushcclosure luaX_pushcclosure
#define lua_pushstring luaX_pushstring
#define lua_pushlstring luaX_pushlstring
#define lua_pushnil luaX_pushnil
#define lua_pushvalue luaX_pushvalue
#define lua_pushfstring luaX_pushfstring

#define lua_toboolean luaX_toboolean
#define lua_tolstring luaX_tolstring
#define lua_touserdata luaX_touserdata
#define lua_tocfunction luaX_tocfunction
#define lua_rawlen luaX_rawlen
#define lua_tonumberx luaX_tonumberx
#define lua_tointegerx luaX_tointegerx
#define lua_isinteger luaX_isinteger
#define lua_iscfunction luaX_iscfunction

#define lua_createtable luaX_createtable
#define lua_rawsetp luaX_rawsetp
#define lua_rawgetp luaX_rawgetp
#define lua_rawset luaX_rawset
#define lua_rawseti luaX_rawseti
#define lua_rawgeti luaX_rawgeti
#define lua_setmetatable luaX_setmetatable
#define lua_setfield luaX_setfield
#define lua_getiuservalue luaX_getiuservalue
#define lua_setiuservalue luaX_setiuservalue

#define lua_gettop luaX_gettop
#define lua_settop luaX_settop
#define lua_rotate luaX_rotate
#define lua_copy luaX_copy

#define luaL_buffinit luaXL_buffinit
#define luaL_prepbuffsize luaXL_prepbuffsize
#define luaL_pushresultsize luaXL_pushresultsize
