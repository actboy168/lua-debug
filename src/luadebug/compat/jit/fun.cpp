#include <compat/internal.h>
#include <lj_obj.h>
ProtoInfo lua_getprotoinfo(lua_State* L, Proto* pt) {
    GCstr* name = proto_chunkname(pt);
    ProtoInfo info;
    info.source          = strdata(name);
    info.linedefined     = pt->firstline;
    info.lastlinedefined = (pt->firstline || !pt->numline) ? pt->firstline + pt->numline : 0;
    return info;
}
