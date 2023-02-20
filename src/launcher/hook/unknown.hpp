#pragma once

#include <dobby.h>
#include <lua.hpp>

#include "hook_common.h"
#include "../symbol_resolver/symbol_resolver.h"

namespace autoattach {
    inline void *getfirstarg(DobbyRegisterContext *ctx) {

#ifdef TARGET_ARCH_ARM64
        return (void*)ctx->general.regs.x0;
#elif defined(TARGET_ARCH_ARM)
        return (void*)ctx->general.regs.r0;
#elif defined(TARGET_ARCH_X64)
#ifdef _WIN32
        return (void*)ctx->general.regs.rcx;
#else
        return (void*)ctx->general.regs.rdi;
#endif
#elif defined(TARGET_ARCH_IA32)
#ifdef _WIN32
        return (void*)ctx->general.regs.ecx;
#else
        return (void*)ctx->general.regs.edi;
#endif
#else
#error "unsupported architecture"
#endif
    }

#define SymbolResolver(name, ...) name = (name##_t)resolver->getsymbol(#name);

#define SymbolResolverWithCheck(name, ...)\
            SymbolResolver(name,)\
            if (!name) return false;

#define Watch(name, func) DobbyInstrument((void*)name, func);
#define UnWatch(name, ...) DobbyDestroy((void*)name);
#define FILED_VAR(name, ...) name##_t name;

    
    void attach_lua_vm(state_t *L);

    struct vmhooker {
        using lua_sethook_t = decltype(&
        ::lua_sethook);
        using lua_gethook_t = decltype(&
        ::lua_gethook);
        using lua_gethookmask_t = decltype(&
        ::lua_gethookmask);
        using lua_gethookcount_t = decltype(&
        ::lua_gethookcount);
#define unknown_vmhook_func(_) \
                _(lua_sethook) \
                _(lua_gethook) \
                _(lua_gethookmask) \
                _(lua_gethookcount)

        unknown_vmhook_func(FILED_VAR)

        lua_Hook origin_lua_hook;
        int origin_hookmask;
        int origin_hookcount;

        void call_origin_hook(lua_State *L, lua_Debug *ar);

        void call_lua_sethook(lua_State *L, lua_Hook fn);

        bool get_symbols(const std::unique_ptr<symbol_resolver::interface> &resolver);
    };
}