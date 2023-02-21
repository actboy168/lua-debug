#pragma once

#include <dobby.h>

#include "hook_common.h"
#include "../symbol_resolver/symbol_resolver.h"
#include "../lua_delayload.h"

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

#define Watch(name, func) DobbyInstrument((void*)name, func);
#define UnWatch(name, ...) DobbyDestroy((void*)name);
#define FILED_VAR(name, ...) name##_t name;

    
    void attach_lua_vm(lua::state L);

    struct vmhooker {
        lua::hook origin_lua_hook;
        int origin_hookmask;
        int origin_hookcount;
        void call_origin_hook(lua::state L, lua::debug ar);
        void call_lua_sethook(lua::state L, lua::hook fn);
        bool get_symbols(const std::unique_ptr<symbol_resolver::interface> &resolver);
    };
}