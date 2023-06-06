#include <compat/internal.h>
#include <lj_obj.h>
ProtoInfo lua_getprotoinfo(lua_State* L, Proto* pt) {
    GCstr* name = proto_chunkname(pt);
    return {
        .source          = strdata(name),
        .linedefined     = pt->firstline,
        .lastlinedefined = (pt->firstline || !pt->numline) ? pt->firstline + pt->numline : 0,
    };
}
