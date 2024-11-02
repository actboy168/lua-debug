
#include <lj_arch.h>
#include <lj_debug.h>
#include <lj_frame.h>
#include <lj_jit.h>
#include <lj_obj.h>
#include <lj_state.h>

#include "compat/internal.h"

extern TValue* index2adr(lua_State* L, int idx);

static cTValue* debug_frame(lua_State* L, int level, int* size) {
    cTValue *frame, *nextframe, *bot = tvref(L->stack) + LJ_FR2;
    /* Traverse frames backwards. */
    for (nextframe = frame = L->base - 1; frame > bot;) {
        if (frame_gc(frame) == obj2gco(L))
            level++; /* Skip dummy frames. See lj_err_optype_call(). */
        if (level-- == 0) {
            *size = (int)(nextframe - frame);
            return frame; /* Level found. */
        }
        nextframe = frame;
        if (frame_islua(frame)) {
            frame = frame_prevl(frame);
        } else {
            if (frame_isvarg(frame))
                level++; /* Skip vararg pseudo-frame. */
            frame = frame_prevd(frame);
        }
    }
    *size = level;
    return NULL; /* Level not found. */
}

#define NO_BCPOS (~(BCPos)0)
static const BCIns* debug_frameins(lua_State* L, GCfunc* fn, cTValue* nextframe) {
    const BCIns* ins;
    lj_assertL(fn->c.gct == ~LJ_TFUNC || fn->c.gct == ~LJ_TTHREAD, "function or frame expected");
    if (!isluafunc(fn)) { /* Cannot derive a PC for non-Lua functions. */
        return nullptr;
    } else if (nextframe == NULL) { /* Lua function on top. */
        void* cf = cframe_raw(L->cframe);
        if (cf == NULL || (char*)cframe_pc(cf) == (char*)cframe_L(cf))
            return nullptr;
        ins = cframe_pc(cf); /* Only happens during error/hook handling. */
    } else {
        if (frame_islua(nextframe)) {
            ins = frame_pc(nextframe);
        } else if (frame_iscont(nextframe)) {
            ins = frame_contpc(nextframe);
        } else {
            /* Lua function below errfunc/gc/hook: find cframe to get the PC. */
            void* cf  = cframe_raw(L->cframe);
            TValue* f = L->base - 1;
            for (;;) {
                if (cf == NULL)
                    return nullptr;
                while (cframe_nres(cf) < 0) {
                    if (f >= restorestack(L, -cframe_nres(cf)))
                        break;
                    cf = cframe_raw(cframe_prev(cf));
                    if (cf == NULL)
                        return nullptr;
                }
                if (f < nextframe)
                    break;
                if (frame_islua(f)) {
                    f = frame_prevl(f);
                } else {
                    if (frame_isc(f) || (frame_iscont(f) && frame_iscont_fficb(f)))
                        cf = cframe_raw(cframe_prev(cf));
                    f = frame_prevd(f);
                }
            }
            ins = cframe_pc(cf);
            if (!ins) return nullptr;
        }
    }
    return ins;
}
/* Return bytecode position for function/frame or NO_BCPOS. */
static BCPos debug_framepc(lua_State* L, GCfunc* fn, cTValue* nextframe) {
    const BCIns* ins = debug_frameins(L, fn, nextframe);
    GCproto* pt;
    BCPos pos;
    if (!ins) return NO_BCPOS;
    pt  = funcproto(fn);
    pos = proto_bcpos(pt, ins) - 1;
#if LJ_HASJIT
    if (pos > pt->sizebc) { /* Undo the effects of lj_trace_exit for JLOOP. */
        GCtrace* T = (GCtrace*)((char*)(ins - 1) - offsetof(GCtrace, startins));
        lj_assertL(bc_isret(bc_op(ins[-1])), "return bytecode expected");
        pos = proto_bcpos(pt, mref(T->startpc, const BCIns));
    }
#endif
    return pos;
}

CallInfo* lua_getcallinfo(lua_State* L) {
    int size;
    return const_cast<CallInfo*>(debug_frame(L, 0, &size));
}

inline Proto* func2proto(GCfunc* func) {
    if (!isluafunc(func))
        return 0;
    return funcproto(func);
}

Proto* lua_getproto(lua_State* L, int idx) {
    GCfunc* func = funcV(index2adr(L, idx));
    return func2proto(func);
}

Proto* lua_ci2proto(CallInfo* ci) {
    GCfunc* func = frame_func(ci);
    return func2proto(func);
}

CallInfo* lua_debug2ci(lua_State* L, const lua_Debug* ar) {
    uint32_t offset = (uint32_t)ar->i_ci & 0xffff;
    return tvref(L->stack) + offset;
}

int lua_isluafunc(lua_State* L, lua_Debug* ar) {
    return isluafunc(frame_func(lua_debug2ci(L, ar)));
}

int lua_getcurrentpc(lua_State* L, CallInfo* ci) {
    return debug_framepc(L, frame_func(ci), ci);
}

const Instruction* lua_getsavedpc(lua_State* L, CallInfo* ci) {
    return debug_frameins(L, frame_func(ci), ci);
}