#include "rdebug_table.h"

#include "rdebug_luacompat.h"

namespace luadebug::table {

    static unsigned int array_limit(const GCtab* t) {
        return t->asize;
    }

    unsigned int array_size(const void* tv) {
        const GCtab* t      = &((const GCobj*)tv)->tab;
        unsigned int alimit = array_limit(t);
        if (alimit) {
            for (unsigned int i = alimit; i > 0; --i) {
                TValue* arr = tvref(t->array);
                if (!tvisnil(&arr[i - 1])) {
                    return i - 1;
                }
            }
        }
        return 0;
    }

    unsigned int hash_size(const void* tv) {
        const GCtab* t = &((const GCobj*)tv)->tab;
        if (t->hmask <= 0) return 0;
        return t->hmask + 1;
    }

    bool has_zero(const void* tv) {
        const GCtab* t = &((const GCobj*)tv)->tab;
        return t->asize > 0 && !tvisnil(arrayslot(t, 0));
    }

    int get_zero(lua_State* L, const void* tv) {
        const GCtab* t = &((const GCobj*)tv)->tab;
        if (t->asize == 0) {
            return 0;
        }
        TValue* v = arrayslot(t, 0);
        if (tvisnil(v)) {
            return 0;
        }
        L->top += 2;
        TValue* key = L->top - 1;
        TValue* val = L->top - 2;
        setintptrV(key, 0);
        copyTV(L, val, v);
        return 1;
    }

    int get_kv(lua_State* L, const void* tv, unsigned int i) {
        const GCtab* t = &((const GCobj*)tv)->tab;

        Node* node = noderef(t->node);
        Node* n    = &node[i];
        if (tvisnil(&n->val)) {
            return 0;
        }
        L->top += 2;
        TValue* key = L->top - 1;
        TValue* val = L->top - 2;
        copyTV(L, key, &n->key);
        copyTV(L, val, &n->val);
        return 1;
    }

    int get_k(lua_State* L, const void* tv, unsigned int i) {
        if (i >= hash_size(tv)) {
            return 0;
        }
        const GCtab* t = &((const GCobj*)tv)->tab;
        Node* node     = noderef(t->node);
        Node* n        = &node[i];
        if (tvisnil(&n->val)) {
            return 0;
        }
        TValue* key = L->top;
        copyTV(L, key, &n->key);
        L->top += 1;
        return 1;
    }

    int get_k(lua_State* L, int idx, unsigned int i) {
        const GCtab* t = &((const GCobj*)lua_topointer(L, idx))->tab;
        if (!t) {
            return 0;
        }
        return get_k(L, t, i);
    }

    int get_v(lua_State* L, int idx, unsigned int i) {
        const GCtab* t = &((const GCobj*)lua_topointer(L, idx))->tab;
        if (!t) {
            return 0;
        }
        if (i >= hash_size(t)) {
            return 0;
        }
        Node* node = noderef(t->node);
        Node* n    = &node[i];
        if (tvisnil(&n->val)) {
            return 0;
        }
        copyTV(L, L->top, &n->val);
        L->top += 1;
        return 1;
    }

    int set_v(lua_State* L, int idx, unsigned int i) {
        const GCtab* t = &((const GCobj*)lua_topointer(L, idx))->tab;
        if (!t) {
            return 0;
        }
        if (i >= hash_size(t)) {
            return 0;
        }
        Node* node = noderef(t->node);
        Node* n    = &node[i];
        if (tvisnil(&n->val)) {
            return 0;
        }
        copyTV(L, &n->val, L->top - 1);
        L->top -= 1;
        return 1;
    }

}
