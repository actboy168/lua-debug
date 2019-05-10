#pragma once

#include <stddef.h>
#include <stdio.h>

extern "C" {

struct rlua_State;
typedef ptrdiff_t rlua_KContext;
typedef int (*rlua_CFunction) (rlua_State *L);
typedef int (*rlua_KFunction) (rlua_State *L, int status, rlua_KContext ctx);

typedef long long rlua_Integer;
typedef unsigned long long rlua_Unsigned;
typedef double rlua_Number;

struct rluaL_Reg {
  const char *name;
  rlua_CFunction func;
};

struct rluaL_Buffer {
    char *b;
    size_t size;
    size_t n;
    rlua_State *L;
    union {
        char b[8192];
    } init;
};

struct rluaL_Stream {
    FILE *f;
    rlua_CFunction closef;
};

rlua_State *(rluaL_newstate) (void);
void (rluaL_openlibs) (rlua_State *L);
int  (rluaL_loadstring) (rlua_State *L, const char *s);
int  (rluaL_loadbufferx) (rlua_State *L, const char *buff, size_t sz, const char *name, const char *mode);
void (rluaL_setfuncs) (rlua_State *L, const rluaL_Reg *l, int nup);
void (rluaL_checktype) (rlua_State *L, int arg, int t);
int  (rluaL_checkoption) (rlua_State *L, int arg, const char *def, const char *const lst[]);
void *(rluaL_checkudata) (rlua_State *L, int ud, const char *tname);
int   (rluaL_newmetatable) (rlua_State *L, const char *tname);
void  (rluaL_setmetatable) (rlua_State *L, const char *tname);
int (rluaL_callmeta) (rlua_State *L, int obj, const char *e);
const char *(rluaL_checklstring) (rlua_State *L, int arg, size_t *l);
rlua_Number  (rluaL_checknumber) (rlua_State *L, int arg);
rlua_Number  (rluaL_optnumber) (rlua_State *L, int arg, rlua_Number def);
rlua_Integer (rluaL_checkinteger) (rlua_State *L, int arg);
rlua_Integer (rluaL_optinteger) (rlua_State *L, int arg, rlua_Integer def);
rlua_Integer (rluaL_len) (rlua_State *L, int idx);

void (rlua_close) (rlua_State *L);
int  (rlua_pcallk) (rlua_State *L, int nargs, int nresults, int errfunc, rlua_KContext ctx, rlua_KFunction k);
void (rlua_callk) (rlua_State *L, int nargs, int nresults, rlua_KContext ctx, rlua_KFunction k);
int  (rlua_type) (rlua_State *L, int idx);
const char *(rlua_typename) (rlua_State *L, int tp);
int  (rlua_error) (rlua_State *L);
int  (rluaL_error) (rlua_State *L, const char *fmt, ...);
void (rluaL_traceback) (rlua_State *L, rlua_State *L1, const char *msg, int level);

void *(rlua_newuserdatauv) (rlua_State *L, size_t sz, int nuvalue);
const char *(rlua_getupvalue) (rlua_State *L, int funcindex, int n);

void  (rlua_pushboolean) (rlua_State *L, int b);
void  (rlua_pushinteger) (rlua_State *L, rlua_Integer n);
void  (rlua_pushnumber) (rlua_State *L, rlua_Number n);
void  (rlua_pushlightuserdata) (rlua_State *L, void *p);
void  (rlua_pushcclosure) (rlua_State *L, rlua_CFunction fn, int n);
const char *(rlua_pushstring) (rlua_State *L, const char *s);
const char *(rlua_pushlstring) (rlua_State *L, const char *s, size_t len);
void  (rlua_pushnil) (rlua_State *L);
void  (rlua_pushvalue) (rlua_State *L, int idx);
const char *(rlua_pushfstring) (rlua_State *L, const char *fmt, ...);

int           (rlua_toboolean) (rlua_State *L, int idx);
const char *  (rlua_tolstring) (rlua_State *L, int idx, size_t *len);
void *        (rlua_touserdata) (rlua_State *L, int idx);
rlua_CFunction (rlua_tocfunction) (rlua_State *L, int idx);
rlua_Unsigned (rlua_rawlen) (rlua_State *L, int idx);
rlua_Number   (rlua_tonumberx) (rlua_State *L, int idx, int *isnum);
rlua_Integer  (rlua_tointegerx) (rlua_State *L, int idx, int *isnum);
int           (rlua_isinteger) (rlua_State *L, int idx);
int           (rlua_iscfunction) (rlua_State *L, int idx);

void (rlua_createtable) (rlua_State *L, int narr, int nrec);
void (rlua_rawsetp) (rlua_State *L, int idx, const void *p);
int  (rlua_rawgetp) (rlua_State *L, int idx, const void *p);
void (rlua_rawset) (rlua_State *L, int idx);
void (rlua_rawseti) (rlua_State *L, int idx, rlua_Integer n);
int  (rlua_rawgeti) (rlua_State *L, int idx, rlua_Integer n);
int  (rlua_setmetatable) (rlua_State *L, int objindex);
void (rlua_setfield) (rlua_State *L, int idx, const char *k);
int  (rlua_getiuservalue) (rlua_State *L, int idx, int n);
int  (rlua_setiuservalue) (rlua_State *L, int idx, int n);

int  (rlua_gettop) (rlua_State *L);
void (rlua_settop) (rlua_State *L, int idx);
void (rlua_rotate) (rlua_State *L, int idx, int n);
void (rlua_copy) (rlua_State *L, int fromidx, int toidx);

void  (rluaL_buffinit) (rlua_State *L, rluaL_Buffer *B);
char *(rluaL_prepbuffsize) (rluaL_Buffer *B, size_t sz);
void  (rluaL_pushresultsize) (rluaL_Buffer *B, size_t sz);

#define rlua_pop(L,n) rlua_settop(L, -(n)-1)
#define rlua_pushcfunction(L,f) rlua_pushcclosure(L, (f), 0)
#define rlua_call(L,n,r) rlua_callk(L, (n), (r), 0, NULL)
#define rlua_pcall(L,n,r,f) rlua_pcallk(L, (n), (r), (f), 0, NULL)
#define rlua_newuserdata(L,s) rlua_newuserdatauv(L,s,1)
#define rlua_newtable(L) rlua_createtable(L, 0, 0)
#define rlua_upvalueindex(i) (LUA_REGISTRYINDEX - (i))
#define rlua_insert(L,idx) rlua_rotate(L, (idx), 1)
#define rluaL_checkstring(L,n) (rluaL_checklstring(L, (n), NULL))
#define rlua_setuservalue(L,idx) rlua_setiuservalue(L,idx,1)
#define rlua_getuservalue(L,idx) rlua_getiuservalue(L,idx,1)
#define rlua_replace(L,idx) (rlua_copy(L, -1, (idx)), rlua_pop(L, 1))
#define rlua_remove(L,idx) (rlua_rotate(L, (idx), -1), rlua_pop(L, 1))
#define rlua_tonumber(L,i) rlua_tonumberx(L,(i),NULL)
#define rlua_tointeger(L,i) rlua_tointegerx(L,(i),NULL)

}
