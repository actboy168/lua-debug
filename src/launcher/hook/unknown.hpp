#pragma once

#include <dobby.h>

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
        void** stack_argument = (void**)((uint64_t)ctx->esp + 4);
        return stack_argument[0];
#else
        return (void*)ctx->general.regs.edi;
#endif
#else
#error "unsupported architecture"
#endif
    }

#define str(s) #s
#define SymbolResolver_lua_(name, prefix) name = (name##_t)resolver->getsymbol(str(prefix##_##name))
#define SymbolResolver_lua(name) SymbolResolver_lua_(name, lua)
#define SymbolResolver_luaL(name) SymbolResolver_lua_(name, luaL)
#define SymbolResolverWithCheck_lua(name) SymbolResolver_lua(name); if (!name) return false;

#define Watch(name, func) DobbyInstrument((void*)name, func);
#define UnWatch(name, ...) DobbyDestroy((void*)name);
#define FILED_VAR(name, ...) name##_t name;

    
    void attach_lua_vm(state_t *L);

    using debug_t = void;
    using hook_t = void (*) (state_t *L, debug_t *ar);

    struct vmhooker {
        using sethook_t = void (*)(state_t *L, hook_t func, int mask, int count);
        using gethook_t = hook_t (*) (state_t *L);
        using gethookmask_t = int (*) (state_t *L);
        using gethookcount_t = int (*) (state_t *L);
#define unknown_vmhook_func(_) \
                _(sethook) \
                _(gethook) \
                _(gethookmask) \
                _(gethookcount)

        unknown_vmhook_func(FILED_VAR)

        hook_t origin_lua_hook;
        int origin_hookmask;
        int origin_hookcount;

        void call_origin_hook(state_t *L, debug_t *ar);

        void call_lua_sethook(state_t *L, hook_t fn);

        bool get_symbols(const std::unique_ptr<symbol_resolver::interface> &resolver);
    };
}