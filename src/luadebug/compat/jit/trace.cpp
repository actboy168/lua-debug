#include <lj_dispatch.h>
#include <lj_jit.h>
#include <lj_obj.h>

#include <lua.hpp>

#if LJ_HASJIT
/* Unpatch the bytecode modified by a root trace. */
static void trace_unpatch(jit_State* J, GCtrace* T) {
    BCOp op   = bc_op(T->startins);
    BCIns* pc = mref(T->startpc, BCIns);
    UNUSED(J);
    if (op == BC_JMP)
        return; /* No need to unpatch branches in parent traces (yet). */
    switch (bc_op(*pc)) {
    case BC_JFORL:
        lj_assertJ(traceref(J, bc_d(*pc)) == T, "JFORL references other trace");
        *pc = T->startins;
        pc += bc_j(T->startins);
        lj_assertJ(bc_op(*pc) == BC_JFORI, "FORL does not point to JFORI");
        setbc_op(pc, BC_FORI);
        break;
    case BC_JITERL:
    case BC_JLOOP:
        lj_assertJ(op == BC_ITERL || op == BC_ITERN || op == BC_LOOP || bc_isret(op), "bad original bytecode %d", op);
        *pc = T->startins;
        break;
    case BC_JMP:
        lj_assertJ(op == BC_ITERL, "bad original bytecode %d", op);
        pc += bc_j(*pc) + 2;
        if (bc_op(*pc) == BC_JITERL) {
            lj_assertJ(traceref(J, bc_d(*pc)) == T, "JITERL references other trace");
            *pc = T->startins;
        }
        break;
    case BC_JFUNCF:
        lj_assertJ(op == BC_FUNCF, "bad original bytecode %d", op);
        *pc = T->startins;
        break;
    default: /* Already unpatched. */
        break;
    }
}
static void trace_flushroot(jit_State* J, GCtrace* T) {
    GCproto* pt = &gcref(T->startpt)->pt;
    lj_assertJ(T->root == 0, "not a root trace");
    lj_assertJ(pt != NULL, "trace has no prototype");
    /* First unpatch any modified bytecode. */
    trace_unpatch(J, T);
    /* Unlink root trace from chain anchored in prototype. */
    if (pt->trace == T->traceno) { /* Trace is first in chain. Easy. */
        pt->trace = T->nextroot;
    }
    else if (pt->trace) { /* Otherwise search in chain of root traces. */
        GCtrace* T2 = traceref(J, pt->trace);
        if (T2) {
            for (; T2->nextroot; T2 = traceref(J, T2->nextroot))
                if (T2->nextroot == T->traceno) {
                    T2->nextroot = T->nextroot; /* Unlink from chain. */
                    break;
                }
        }
    }
}
void lj_trace_flushproto(global_State* g, GCproto* pt) {
    while (pt->trace != 0)
        trace_flushroot(G2J(g), traceref(G2J(g), pt->trace));
}

/* Re-enable compiling a prototype by unpatching any modified bytecode. */
void lj_trace_reenableproto(GCproto* pt) {
    if ((pt->flags & PROTO_ILOOP)) {
        BCIns* bc = proto_bc(pt);
        BCPos i, sizebc = pt->sizebc;
        pt->flags &= ~PROTO_ILOOP;
        if (bc_op(bc[0]) == BC_IFUNCF)
            setbc_op(&bc[0], BC_FUNCF);
        for (i = 1; i < sizebc; i++) {
            BCOp op = bc_op(bc[i]);
            if (op == BC_IFORL || op == BC_IITERL || op == BC_ILOOP)
                setbc_op(&bc[i], (int)op + (int)BC_LOOP - (int)BC_ILOOP);
        }
    }
}

/* Set JIT mode for a single prototype. */
static void setptmode(global_State* g, GCproto* pt, int mode) {
    if ((mode & LUAJIT_MODE_ON)) { /* (Re-)enable JIT compilation. */
        pt->flags &= ~PROTO_NOJIT;
        lj_trace_reenableproto(pt); /* Unpatch all ILOOP etc. bytecodes. */
    }
    else { /* Flush and/or disable JIT compilation. */
        if (!(mode & LUAJIT_MODE_FLUSH))
            pt->flags |= PROTO_NOJIT;
        lj_trace_flushproto(g, pt); /* Flush all traces of prototype. */
    }
}

/* Recursively set the JIT mode for all children of a prototype. */
static void setptmode_all(global_State* g, GCproto* pt, int mode) {
    ptrdiff_t i;
    if (!(pt->flags & PROTO_CHILD)) return;
    for (i = -(ptrdiff_t)pt->sizekgc; i < 0; i++) {
        GCobj* o = proto_kgc(pt, i);
        if (o->gch.gct == ~LJ_TPROTO) {
            setptmode(g, gco2pt(o), mode);
            setptmode_all(g, gco2pt(o), mode);
        }
    }
}
#endif

bool luajit_set_jitmode(lua_State* L, GCproto* pt, bool enable) {
#ifdef LJ_HASJIT
    bool nojit = pt->flags & PROTO_NOJIT;
    if (enable) {
        if (nojit)
            setptmode_all(G(L), pt, LUAJIT_MODE_ON);
    }
    else {
        if (!nojit)
            setptmode_all(G(L), pt, LUAJIT_MODE_OFF);
    }
    return nojit;
#else
    return false;
#endif
}
