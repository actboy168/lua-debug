#pragma once

#include <assert.h>
#include <bee/nonstd/bit.h>
#include <bee/nonstd/to_underlying.h>
#include <bee/nonstd/unreachable.h>
#include <bee/utility/zstring_view.h>

#include <stdexcept>
#include <type_traits>

#include "rdebug_debughost.h"
#include "rdebug_lua.h"

namespace luadebug {
    struct protected_area {
        using visitor = int (*)(luadbg_State* L, lua_State* cL);
        static int raise_error(luadbg_State* L, lua_State* cL, const char* msg) {
            leave(L, cL);
            luadbg_pushstring(L, msg);
            return luadbg_error(L);
        }

        static inline void check_type(luadbg_State* L, int arg, int t) {
            if (luadbg_type(L, arg) != t) {
                leave(L);
                luadbgL_typeerror(L, arg, luadbg_typename(L, t));
            }
        }

        template <typename T, typename I>
        static inline constexpr bool checklimit(I i) {
            static_assert(std::is_integral_v<I>);
            static_assert(std::is_integral_v<T>);
            static_assert(sizeof(I) >= sizeof(T));
            if constexpr (sizeof(I) == sizeof(T)) {
                return true;
            }
            else if constexpr (std::numeric_limits<I>::is_signed == std::numeric_limits<T>::is_signed) {
                return i >= std::numeric_limits<T>::lowest() && i <= (std::numeric_limits<T>::max)();
            }
            else if constexpr (std::numeric_limits<I>::is_signed) {
                return static_cast<std::make_unsigned_t<I>>(i) >= std::numeric_limits<T>::lowest() && static_cast<std::make_unsigned_t<I>>(i) <= (std::numeric_limits<T>::max)();
            }
            else {
                return static_cast<std::make_signed_t<I>>(i) >= std::numeric_limits<T>::lowest() && static_cast<std::make_signed_t<I>>(i) <= (std::numeric_limits<T>::max)();
            }
        }

        template <typename T>
        static inline T checkinteger(luadbg_State* L, int arg) {
            static_assert(std::is_trivial_v<T>);
            if constexpr (std::is_enum_v<T>) {
                using UT = std::underlying_type_t<T>;
                return static_cast<T>(checkinteger<UT>(L, arg));
            }
            else if constexpr (sizeof(T) != sizeof(luadbg_Integer)) {
                static_assert(std::is_integral_v<T>);
                static_assert(sizeof(T) < sizeof(luadbg_Integer));
                luadbg_Integer r = checkinteger<luadbg_Integer>(L, arg);
                if (checklimit<T>(r)) {
                    return static_cast<T>(r);
                }
                leave(L);
                luadbgL_error(L, "bad argument '#%d' limit exceeded", arg);
                std::unreachable();
            }
            else if constexpr (!std::is_same_v<T, luadbg_Integer>) {
                return std::bit_cast<T>(checkinteger<luadbg_Integer>(L, arg));
            }
            else {
                int isnum;
                luadbg_Integer d = luadbg_tointegerx(L, arg, &isnum);
                if (!isnum) {
                    leave(L);
                    if (luadbg_isnumber(L, arg))
                        luadbgL_argerror(L, arg, "number has no integer representation");
                    else
                        luadbgL_typeerror(L, arg, luadbg_typename(L, LUA_TNUMBER));
                }
                return d;
            }
        }

        template <typename T>
        static T optinteger(luadbg_State* L, int arg, T def) {
            static_assert(std::is_trivial_v<T>);
            if constexpr (std::is_enum_v<T>) {
                using UT = std::underlying_type_t<T>;
                return static_cast<T>(optinteger<UT>(L, arg, std::to_underlying(def)));
            }
            else if constexpr (sizeof(T) != sizeof(luadbg_Integer)) {
                static_assert(std::is_integral_v<T>);
                static_assert(sizeof(T) < sizeof(luadbg_Integer));
                luadbg_Integer r = optinteger<luadbg_Integer>(L, arg, static_cast<luadbg_Integer>(def));
                if (checklimit<T>(r)) {
                    return static_cast<T>(r);
                }
                leave(L);
                luadbgL_error(L, "bad argument '#%d' limit exceeded", arg);
                std::unreachable();
            }
            else if constexpr (!std::is_same_v<T, luadbg_Integer>) {
                return std::bit_cast<T>(optinteger<luadbg_Integer>(L, arg, std::bit_cast<luadbg_Integer>(def)));
            }
            else {
                return luadbgL_optinteger(L, arg, def);
            }
        }

        static inline bee::zstring_view checkstring(luadbg_State* L, int arg) {
            size_t sz;
            const char* s = luadbg_tolstring(L, arg, &sz);
            if (!s) {
                leave(L);
                luadbgL_typeerror(L, arg, luadbg_typename(L, LUA_TSTRING));
            }
            return { s, sz };
        }

        static inline int call(luadbg_State* L, visitor func) {
            lua_State* cL = entry(L);
            try {
                int r = func(L, cL);
                check_leave(L, cL);
                return r;
            } catch (const std::exception& e) {
                fprintf(stderr, "catch std::exception: %s\n", e.what());
                leave(L, cL);
                return 0;
            } catch (...) {
                fprintf(stderr, "catch unknown exception\n");
                leave(L, cL);
                return 0;
            }
        }

    private:
        static inline int CLIENT_TOP;
        static inline lua_State* entry(luadbg_State* L) {
            if (luadbg_rawgetp(L, LUADBG_REGISTRYINDEX, &CLIENT_TOP) != LUA_TNIL) {
                luadbg_Integer top = luadbg_tointeger(L, -1);
                if (top >= 0) {
                    luadbgL_error(L, "can't recursive");
                }
            }
            luadbg_pop(L, 1);
            lua_State* cL = debughost::get(L);
            luadbg_pushinteger(L, (luadbg_Integer)lua_gettop(cL));
            luadbg_rawsetp(L, LUADBG_REGISTRYINDEX, &CLIENT_TOP);
            return cL;
        }
        static inline void check_leave(luadbg_State* L, lua_State* cL) {
            if (luadbg_rawgetp(L, LUADBG_REGISTRYINDEX, &CLIENT_TOP) == LUA_TNIL) {
                luadbgL_error(L, "not expected");
                return;
            }
            luadbg_Integer top = luadbg_tointeger(L, -1);
            luadbg_pop(L, 1);
            if (top != lua_gettop(cL)) {
                luadbgL_error(L, "not expected");
            }
            luadbg_pushnil(L);
            luadbg_rawsetp(L, LUADBG_REGISTRYINDEX, &CLIENT_TOP);
        }
        static inline void leave(luadbg_State* L, lua_State* cL) {
            if (luadbg_rawgetp(L, LUADBG_REGISTRYINDEX, &CLIENT_TOP) == LUA_TNIL) {
                luadbg_pop(L, 1);
                return;
            }
            luadbg_Integer top = luadbg_tointeger(L, -1);
            luadbg_pop(L, 1);
            if (top >= 0) {
                lua_settop(cL, static_cast<int>(top));
            }
            luadbg_pushnil(L);
            luadbg_rawsetp(L, LUADBG_REGISTRYINDEX, &CLIENT_TOP);
        }
        static inline void leave(luadbg_State* L) {
            if (luadbg_rawgetp(L, LUADBG_REGISTRYINDEX, &CLIENT_TOP) == LUA_TNIL) {
                luadbg_pop(L, 1);
                return;
            }
            luadbg_Integer top = luadbg_tointeger(L, -1);
            luadbg_pop(L, 1);
            if (top >= 0) {
                lua_State* cL = debughost::get(L);
                lua_settop(cL, static_cast<int>(top));
            }
            luadbg_pushnil(L);
            luadbg_rawsetp(L, LUADBG_REGISTRYINDEX, &CLIENT_TOP);
        }
    };

    template <protected_area::visitor func>
    static int protected_call(luadbg_State* L) {
        return protected_area::call(L, func);
    }
}
