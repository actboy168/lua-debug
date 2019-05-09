#pragma once

#include <stddef.h>
#include <stdio.h>

extern "C" {

struct luaX_State;
typedef ptrdiff_t luaX_KContext;
typedef int (*luaX_CFunction) (luaX_State *L);
typedef int (*luaX_KFunction) (luaX_State *L, int status, luaX_KContext ctx);

typedef long long luaX_Integer;
typedef unsigned long long luaX_Unsigned;
typedef double luaX_Number;

struct luaXL_Reg {
  const char *name;
  luaX_CFunction func;
};

struct luaXL_Buffer {
    char *b;
    size_t size;
    size_t n;
    luaX_State *L;
    union {
        char b[8192];
    } init;
};

struct luaXL_Stream {
    FILE *f;
    luaX_CFunction closef;
};

luaX_State *(luaXL_newstate) (void);
void (luaXL_openlibs) (luaX_State *L);
int  (luaXL_loadstring) (luaX_State *L, const char *s);
int  (luaXL_loadbufferx) (luaX_State *L, const char *buff, size_t sz, const char *name, const char *mode);
void (luaXL_setfuncs) (luaX_State *L, const luaXL_Reg *l, int nup);
void (luaXL_checktype) (luaX_State *L, int arg, int t);
int  (luaXL_checkoption) (luaX_State *L, int arg, const char *def, const char *const lst[]);
void *(luaXL_checkudata) (luaX_State *L, int ud, const char *tname);
int   (luaXL_newmetatable) (luaX_State *L, const char *tname);
void  (luaXL_setmetatable) (luaX_State *L, const char *tname);
int (luaXL_callmeta) (luaX_State *L, int obj, const char *e);
const char *(luaXL_checklstring) (luaX_State *L, int arg, size_t *l);
luaX_Number  (luaXL_checknumber) (luaX_State *L, int arg);
luaX_Number  (luaXL_optnumber) (luaX_State *L, int arg, luaX_Number def);
luaX_Integer (luaXL_checkinteger) (luaX_State *L, int arg);
luaX_Integer (luaXL_optinteger) (luaX_State *L, int arg, luaX_Integer def);
luaX_Integer (luaXL_len) (luaX_State *L, int idx);

void (luaX_close) (luaX_State *L);
int  (luaX_pcallk) (luaX_State *L, int nargs, int nresults, int errfunc, luaX_KContext ctx, luaX_KFunction k);
void (luaX_callk) (luaX_State *L, int nargs, int nresults, luaX_KContext ctx, luaX_KFunction k);
int  (luaX_type) (luaX_State *L, int idx);
const char *(luaX_typename) (luaX_State *L, int tp);
int  (luaX_error) (luaX_State *L);
int  (luaXL_error) (luaX_State *L, const char *fmt, ...);
void (luaXL_traceback) (luaX_State *L, luaX_State *L1, const char *msg, int level);

void *(luaX_newuserdatauv) (luaX_State *L, size_t sz, int nuvalue);
const char *(luaX_getupvalue) (luaX_State *L, int funcindex, int n);

void  (luaX_pushboolean) (luaX_State *L, int b);
void  (luaX_pushinteger) (luaX_State *L, luaX_Integer n);
void  (luaX_pushnumber) (luaX_State *L, luaX_Number n);
void  (luaX_pushlightuserdata) (luaX_State *L, void *p);
void  (luaX_pushcclosure) (luaX_State *L, luaX_CFunction fn, int n);
const char *(luaX_pushstring) (luaX_State *L, const char *s);
const char *(luaX_pushlstring) (luaX_State *L, const char *s, size_t len);
void  (luaX_pushnil) (luaX_State *L);
void  (luaX_pushvalue) (luaX_State *L, int idx);
const char *(luaX_pushfstring) (luaX_State *L, const char *fmt, ...);

int           (luaX_toboolean) (luaX_State *L, int idx);
const char *  (luaX_tolstring) (luaX_State *L, int idx, size_t *len);
void *        (luaX_touserdata) (luaX_State *L, int idx);
luaX_CFunction (luaX_tocfunction) (luaX_State *L, int idx);
luaX_Unsigned (luaX_rawlen) (luaX_State *L, int idx);
luaX_Number   (luaX_tonumberx) (luaX_State *L, int idx, int *isnum);
luaX_Integer  (luaX_tointegerx) (luaX_State *L, int idx, int *isnum);
int           (luaX_isinteger) (luaX_State *L, int idx);
int           (luaX_iscfunction) (luaX_State *L, int idx);

void (luaX_createtable) (luaX_State *L, int narr, int nrec);
void (luaX_rawsetp) (luaX_State *L, int idx, const void *p);
int  (luaX_rawgetp) (luaX_State *L, int idx, const void *p);
void (luaX_rawset) (luaX_State *L, int idx);
void (luaX_rawseti) (luaX_State *L, int idx, luaX_Integer n);
int  (luaX_rawgeti) (luaX_State *L, int idx, luaX_Integer n);
int  (luaX_setmetatable) (luaX_State *L, int objindex);
void (luaX_setfield) (luaX_State *L, int idx, const char *k);
int  (luaX_getiuservalue) (luaX_State *L, int idx, int n);
int  (luaX_setiuservalue) (luaX_State *L, int idx, int n);

int  (luaX_gettop) (luaX_State *L);
void (luaX_settop) (luaX_State *L, int idx);
void (luaX_rotate) (luaX_State *L, int idx, int n);
void (luaX_copy) (luaX_State *L, int fromidx, int toidx);

void  (luaXL_buffinit) (luaX_State *L, luaXL_Buffer *B);
char *(luaXL_prepbuffsize) (luaXL_Buffer *B, size_t sz);
void  (luaXL_pushresultsize) (luaXL_Buffer *B, size_t sz);

#define luaX_pop(L,n) luaX_settop(L, -(n)-1)
#define luaX_pushcfunction(L,f) luaX_pushcclosure(L, (f), 0)
#define luaX_call(L,n,r) luaX_callk(L, (n), (r), 0, NULL)
#define luaX_pcall(L,n,r,f) luaX_pcallk(L, (n), (r), (f), 0, NULL)
#define luaX_newuserdata(L,s) luaX_newuserdatauv(L,s,1)
#define luaX_newtable(L) luaX_createtable(L, 0, 0)
#define luaX_upvalueindex(i) (LUA_REGISTRYINDEX - (i))
#define luaX_insert(L,idx) luaX_rotate(L, (idx), 1)
#define luaXL_checkstring(L,n) (luaXL_checklstring(L, (n), NULL))
#define luaX_setuservalue(L,idx) luaX_setiuservalue(L,idx,1)
#define luaX_getuservalue(L,idx) luaX_getiuservalue(L,idx,1)
#define luaX_replace(L,idx) (luaX_copy(L, -1, (idx)), luaX_pop(L, 1))
#define luaX_remove(L,idx) (luaX_rotate(L, (idx), -1), luaX_pop(L, 1))
#define luaX_tonumber(L,i) luaX_tonumberx(L,(i),NULL)
#define luaX_tointeger(L,i) luaX_tointegerx(L,(i),NULL)

}
