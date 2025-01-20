#include <compat/internal.h>
#include <lj_obj.h>

void lua_foreach_gcobj(lua_State* L, lua_foreach_gcobj_cb cb, void* ud) {
    auto g = G(L);
    auto p = gcref(g->gc.root);
    while (p != NULL) {
        cb(L, ud, p);
        p = gcref(p->gch.nextgc);
    }
}

Proto* lua_toproto(GCobject* o) {
    if (o->gch.gct == ~LJ_TPROTO) {
        return gco2pt(o);
    }
    return nullptr;
}
