// clang-format off
#include "luadbg/inc/luadbgexports.h"

#if defined(__linux__)
#    define LUA_USE_LINUX
#elif defined(__APPLE__)
#    define LUA_USE_MACOSX
#endif

#define LUAI_MAXCCALLS 1000

/* no need to change anything below this line ----------------------------- */
#ifdef _WIN32
#define LUA_BUILD_AS_DLL
#endif
#define LUA_CORE
#define LUA_LIB
#include "lprefix.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* setup for luaconf.h */
#define ltable_c
#define lvm_c
#include "luaconf.h"

/* do not export internal symbols */
#undef LUAI_FUNC
#undef LUAI_DDEC
#undef LUAI_DDEF
#define LUAI_FUNC static
#define LUAI_DDEC(def) /* empty */
#define LUAI_DDEF static

/* core -- used by all */
#include "lzio.c"
#include "lctype.c"
#include "lopcodes.c"
#include "lmem.c"
// #include "lundump.c"
#include "ldump.c"
#include "lstate.c"
#include "lgc.c"
#include "llex.c"
#include "lcode.c"
#include "lparser.c"
#include "ldebug.c"
#include "lfunc.c"
#include "lobject.c"
#include "ltm.c"
#include "lstring.c"
#include "ltable.c"
#include "ldo.c"
#include "lvm.c"
#include "lapi.c"

/* auxiliary library -- used by all */
#include "lauxlib.c"

/* standard library  -- not used by luac */
#include "lbaselib.c"
#include "lcorolib.c"
#include "ldblib.c"
#include "liolib.c"
#include "lmathlib.c"
#include "loadlib.c"
#include "loslib.c"
#include "lstrlib.c"
#include "ltablib.c"
#include "lutf8lib.c"
#include "linit.c"

LClosure* luaU_undump(lua_State* L, ZIO* Z, const char* name) {
    luaO_pushfstring(L, "%s: binary loader not available", name);
    luaD_throw(L, LUA_ERRSYNTAX);
    return NULL;
}

void _bee_lua_assert(const char* message, const char* file, unsigned line) {
    fprintf(stderr, "(%s:%d) %s\n", file, line, message);
    fflush(stderr);
    abort();
}

void _bee_lua_apicheck(void* l, const char* message, const char* file, unsigned line) {
    lua_State* L = l;
    fprintf(stderr, "(%s:%d) %s\n", file, line, message);
    fflush(stderr);
    if (!lua_checkstack(L, LUA_MINSTACK)) {
        abort();
    }
    luaL_traceback(L, L, 0, 0);
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    fflush(stderr);
    lua_pop(L, 1);
    abort();
}
