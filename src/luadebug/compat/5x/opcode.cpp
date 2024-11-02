#include <bee/nonstd/format.h>
#include <ctype.h>
#include <ldebug.h>
#include <stdint.h>
#define lprefix_h 1
#include <lopcodes.c>

#include "compat/internal.h"

#if LUA_VERSION_NUM == 504
#    include <lopnames.h>
const auto& luaP_opnames = opnames;
static int getbaseline(const Proto* f, int pc, int* basepc) {
    if (f->sizeabslineinfo == 0 || pc < f->abslineinfo[0].pc) {
        *basepc = -1; /* start from the beginning */
        return f->linedefined;
    } else {
        int i = cast_uint(pc) / MAXIWTHABS - 1; /* get an estimate */
        /* estimate must be a lower bound of the correct base */
        lua_assert(i < 0 || (i < f->sizeabslineinfo && f->abslineinfo[i].pc <= pc));
        while (i + 1 < f->sizeabslineinfo && pc >= f->abslineinfo[i + 1].pc)
            i++; /* low estimate; adjust it */
        *basepc = f->abslineinfo[i].pc;
        return f->abslineinfo[i].line;
    }
}

int luaG_getfuncline(const Proto* f, int pc) {
    if (f->lineinfo == NULL) /* no debug information? */
        return -1;
    else {
        int basepc;
        int baseline = getbaseline(f, pc, &basepc);
        while (basepc++ < pc) { /* walk until given instruction */
            lua_assert(f->lineinfo[basepc] != ABSLINEINFO);
            baseline += f->lineinfo[basepc]; /* correct line */
        }
        return baseline;
    }
}
#endif

#define TOVOID(p) ((const void*)(p))
#define MYK(x) (-1 - (x))

int LuaOpCode::line(Proto* f) const {
#if LUA_VERSION_NUM == 501
    return getline(f, pc);
#elif LUA_VERSION_NUM == 502 || LUA_VERSION_NUM == 503
    return getfuncline(f, pc);
#elif LUA_VERSION_NUM == 504
    return luaG_getfuncline(f, pc);
#endif
}

std::string LuaOpCode::tostring() const {
    std::string ret { name };
    ret.push_back('\t');
#if LUA_VERSION_NUM == 504
    switch (o) {
    case OP_UNM:
    case OP_BNOT:
    case OP_NOT:
    case OP_LEN:
    case OP_CONCAT:
    case OP_SETUPVAL:
    case OP_GETUPVAL:
    case OP_LOADNIL:
    case OP_MOVE:
        ret += std::format("{} {}", a, b);
        break;
    case OP_LOADF:
    case OP_LOADI:
        ret += std::format("{} {}", a, sbx);
        break;
    case OP_LOADK:
        ret += std::format("{} {}", a, bx);
        break;
    case OP_LOADFALSE:
    case OP_LFALSESKIP:
    case OP_LOADTRUE:
    case OP_LOADKX:
    case OP_CLOSE:
    case OP_TBC:
        ret += std::format("{}", a);
        break;
    case OP_GETFIELD:
    case OP_NEWTABLE:
    case OP_GETI:
    case OP_ADDK:
    case OP_SUBK:
    case OP_MULK:
    case OP_POWK:
    case OP_MODK:
    case OP_GETTABLE:
    case OP_BANDK:
    case OP_DIVK:
    case OP_BORK:
    case OP_BXORK:
    case OP_IDIVK:
    case OP_GETTABUP:
    case OP_ADD:
    case OP_SUB:
    case OP_MUL:
    case OP_MOD:
    case OP_POW:
    case OP_DIV:
    case OP_IDIV:
    case OP_BAND:
    case OP_BOR:
    case OP_BXOR:
    case OP_SHL:
    case OP_SHR:
    case OP_MMBIN:
    case OP_SETLIST:
    case OP_CALL:
        ret += std::format("{} {} {}", a, b, c);
        break;
    case OP_SETFIELD:
    case OP_SETI:
    case OP_SETTABLE:
    case OP_SELF:
    case OP_SETTABUP:
#    define ISK (isk ? "k" : "")
        ret += std::format("{} {} {}{}", a, b, c, ISK);
        break;
    case OP_SHRI:
    case OP_SHLI:
    case OP_ADDI:
        ret += std::format("{} {} {}", a, b, sc);
        break;
    case OP_MMBINI:
        ret += std::format("{} {} {} {}", a, sb, c, isk);
        break;
    case OP_MMBINK:
        ret += std::format("{} {} {} {}", a, b, c, isk);
        break;
    case OP_JMP:
        ret += std::format("{}", GETARG_sJ(*ins));
        break;
    case OP_EQ:
    case OP_LT:
    case OP_LE:
    case OP_EQK:
        ret += std::format("{} {} {}", a, b, isk);
        break;
    case OP_EQI:
    case OP_LTI:
    case OP_LEI:
    case OP_GTI:
    case OP_GEI:
        ret += std::format("{} {} {}", a, sb, isk);
        break;
    case OP_TEST:
        ret += std::format("{} {}", a, isk);
        break;
    case OP_TESTSET:
        ret += std::format("{} {} {}", a, b, isk);
        break;
    case OP_TAILCALL:
    case OP_RETURN:
        ret += std::format("{} {} {}{}", a, b, c, ISK);
        break;
    case OP_RETURN0:
        break;
    case OP_RETURN1:
    case OP_VARARGPREP:
        ret += std::format("{}", a);
        break;
    case OP_FORLOOP:
    case OP_FORPREP:
    case OP_TFORLOOP:
    case OP_CLOSURE:
    case OP_TFORPREP:
        ret += std::format("{} {}", a, bx);
        break;
    case OP_TFORCALL:
    case OP_VARARG:
        ret += std::format("{} {}", a, c);
        break;
    case OP_EXTRAARG:
        ret += std::format("{}", ax);
        break;
#    if 0
      default:
            printf("%d %d %d",a,b,c);
            printf(COMMENT "not handled");
            break;
#    endif
    }

#else
    switch (getOpMode(o)) {
    case iABC:
        ret += std::format("{}{}{}", a, getBMode(o) != OpArgN ? std::format(" {}", ISK(b) ? (MYK(INDEXK(b))) : b) : "", getCMode(o) != OpArgN ? std::format(" {}", ISK(c) ? (MYK(INDEXK(c))) : c) : "");
        break;
    case iABx:
#    if LUA_VERSION_NUM == 501
        ret += std::format("{} {}", a, (getBMode(o) == OpArgK) ? MYK(bx) : bx);
#    else
        ret += std::format("{} {}{}", a, (getBMode(o) == OpArgK) ? std::format(" {}", MYK(bx)) : "", (getBMode(o) == OpArgU) ? std::format(" {}", bx) : "");
#    endif
        break;
    case iAsBx:
#    if LUA_VERSION_NUM == 501
        ret += std::format("{}", o == OP_JMP ? std::format("{}", sbx) : std::format("{} {}", a, sbx));
#    else
        ret += std::format("{} {}", a, sbx);
#    endif
        break;
#    if LUA_VERSION_NUM > 501
    case iAx:
        ret += std::format("{}", MYK(ax));
        break;

#    endif
    }
#endif
    return ret;
}

bool lua_getopcode(Proto* f, lua_Integer pc, LuaOpCode& ret) {
    if (pc >= f->sizecode) return false;
    const auto i = f->code[pc];
    ret.o        = GET_OPCODE(i);
    ret.a        = GETARG_A(i);
    ret.b        = GETARG_B(i);
    ret.c        = GETARG_C(i);
#if LUA_VERSION_NUM > 501
    ret.ax = GETARG_Ax(i);
#    if LUA_VERSION_NUM == 504
    ret.sb  = GETARG_sB(i);
    ret.sc  = GETARG_sC(i);
    ret.isk = GETARG_k(i);
#    endif
#endif
    ret.bx   = GETARG_Bx(i);
    ret.sbx  = GETARG_sBx(i);
    ret.pc   = 0;
    ret.name = luaP_opnames[ret.o];
    ret.ins  = &(f->code[pc]);
    return true;
}
