#include "rdebug_table.h"
#include "rluaobject.h"

#ifdef LUAJIT_VERSION
#include "lj_tab.h"
cTValue * LJ_FASTCALL lj_tab_getinth(GCtab *t, int32_t key)
{
  TValue k;
  Node *n;
  k.n = (lua_Number)key;
  n = hashnum(t, &k);
  do {
    if (tvisnum(&n->key) && n->key.n == k.n)
      return &n->val;
  } while ((n = nextnode(n)));
  return NULL;
}
LJ_NOINLINE static MSize tab_len_slow(GCtab *t, size_t hi)
{
  cTValue *tv;
  size_t lo = hi;
  hi++;
  /* Widening search for an upper bound. */
  while ((tv = lj_tab_getint(t, (int32_t)hi)) && !tvisnil(tv)) {
    lo = hi;
    hi += hi;
    if (hi > (size_t)(INT_MAX-2)) {  /* Punt and do a linear search. */
      lo = 1;
      while ((tv = lj_tab_getint(t, (int32_t)lo)) && !tvisnil(tv)) lo++;
      return (MSize)(lo - 1);
    }
  }
  /* Binary search to find a non-nil to nil transition. */
  while (hi - lo > 1) {
    size_t mid = (lo+hi) >> 1;
    cTValue *tvb = lj_tab_getint(t, (int32_t)mid);
    if (tvb && !tvisnil(tvb)) lo = mid; else hi = mid;
  }
  return (MSize)lo;
}
MSize LJ_FASTCALL lj_tab_len(GCtab *t)
{
  size_t hi = (size_t)t->asize;
  if (hi) hi--;
  /* In a growing array the last array element is very likely nil. */
  if (hi > 0 && LJ_LIKELY(tvisnil(arrayslot(t, hi)))) {
    /* Binary search to find a non-nil to nil transition in the array. */
    size_t lo = 0;
    while (hi - lo > 1) {
      size_t mid = (lo+hi) >> 1;
      if (tvisnil(arrayslot(t, mid))) hi = mid; else lo = mid;
    }
    return (MSize)lo;
  }
  /* Without a hash part, there's an implicit nil after the last element. */
  return t->hmask ? tab_len_slow(t, hi) : (MSize)hi;
}
#endif

namespace remotedebug::table {

#if LUA_VERSION_NUM < 504
#define s2v(o) (o)
#endif


static unsigned int array_limit(const Table* t) {
#if LUA_VERSION_NUM >= 504
    if ((!(t->marked & BITRAS) || (t->alimit & (t->alimit - 1)) == 0)) {
        return t->alimit;
    }
    unsigned int size = t->alimit;
    size |= (size >> 1);
    size |= (size >> 2);
    size |= (size >> 4);
    size |= (size >> 8);
    size |= (size >> 16);
#if (UINT_MAX >> 30) > 3
    size |= (size >> 32);
#endif
    size++;
    return size;
#elif defined(LUAJIT_VERSION)
	unsigned int result = t->asize;
	if(result) result--;
	return result;
#else
    return t->sizearray;
#endif
}

unsigned int array_size(const void* tv) {
#ifdef LUAJIT_VERSION
	return lj_tab_len((Table*)tv);
#else 
	const Table* t = (const Table*)tv;
	unsigned int alimit = array_limit(t);
	if (alimit) {
		for (unsigned int i = alimit; i > 0; --i) {
			if (!ttisnil(&t->array[i-1])) {
				return i;
			}
		}
	}
	return 0;
#endif
}

unsigned int hash_size(const void* tv) {
	const Table* t = (const Table*)tv;
#ifdef LUAJIT_VERSION
	return t->hmask + 1;
#else
	return (unsigned int)(1<<t->lsizenode);
#endif
}

int get_kv(lua_State* L, const void* tv, unsigned int i) {
	const Table* t = (const Table*) tv;

#ifdef LUAJIT_VERSION
	Node* node = noderef(t->node);
	Node* n = &node[i];
	if(tvisnil(&n->val))
	{
		return 0;
	}
	L->top += 2;
	StkId key = L->top - 1;
	StkId val = L->top - 2;
	copyTV(L,key, &n->key);
	copyTV(L,val, &n->val);
#else
	Node* n = &t->node[i];
	if (ttisnil(gval(n))) {
		return 0;
	}

	L->top += 2;
	StkId key = L->top - 1;
	StkId val = L->top - 2;
#if LUA_VERSION_NUM >= 504
	getnodekey(L, s2v(key), n);
#else
	setobj2s(L, key, &n->i_key.tvk);
#endif
	setobj2s(L, val, gval(n));
#endif
	return 1;
}

int get_k(lua_State* L, const void* t, unsigned int i) {
	if (i >= hash_size(t)) {
		return 0;
	}
#ifdef LUAJIT_VERSION
	const Table* ct = (const Table*)t;
	Node* node = noderef(ct->node);
	Node* n = &node[i];
	if(tvisnil(&n->val))
	{
		return 0;
	}
	StkId key = L->top;
	copyTV(L,key,&n->key);
#else
	Node* n = &((const Table*)t)->node[i];
	if (ttisnil(gval(n))) {
		return 0;
	}
	StkId key = L->top;
#if LUA_VERSION_NUM >= 504
	getnodekey(L, s2v(key), n);
#else
	setobj2s(L, key, &n->i_key.tvk);
#endif
#endif
	L->top++;
	return 1;
}

int get_k(lua_State* L, int idx, unsigned int i) {
#ifdef LUAJIT_VERSION
	const GCtab* t = &((const GCobj*)lua_topointer(L, idx))->tab;
#else
	const void* t = lua_topointer(L,idx);
#endif
	if (!t) {
		return 0;
	}
	return get_k(L, t, i);
}

int get_v(lua_State* L, int idx, unsigned int i) {
#ifdef LUAJIT_VERSION
	const GCtab* t = &((const GCobj*)lua_topointer(L, idx))->tab;
#else
	const Table* t = (const Table*)lua_topointer(L, idx);
#endif
	if (!t) {
		return 0;
	}
	if (i >= hash_size(t)) {
		return 0;
	}
#ifdef LUAJIT_VERSION
	Node* node = noderef(t->node);
	Node* n = &node[i];
	if(tvisnil(&n->val))
	{
		return 0;
	}
	copyTV(L,L->top,&n->val);
#else
	Node* n = &t->node[i];
	if (ttisnil(gval(n))) {
		return 0;
	}
	setobj2s(L, L->top, gval(n));
#endif
	L->top++;
	return 1;
}

int set_v(lua_State* L, int idx, unsigned int i) {
#ifdef LUAJIT_VERSION
	const GCtab* t = &((const GCobj*)lua_topointer(L, idx))->tab;
#else
	const Table* t = (const Table*)lua_topointer(L, idx);
#endif
	if (!t) {
		return 0;
	}
	if (i >= hash_size(t)) {
		return 0;
	}

#ifdef LUAJIT_VERSION
	Node* node = noderef(t->node);
	Node* n = &node[i];
	if(tvisnil(&n->val))
	{
		return 0;
	}
	copyTV(L,&n->val,L->top - 1);
#else
	Node* n = &t->node[i];
	if (ttisnil(gval(n))) {
		return 0;
	}
	setobj2t(L, gval(n), s2v(L->top - 1));
#endif
	L->top--;
	return 1;
}

}
