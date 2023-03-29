#pragma once
#include <autoattach/autoattach.h>
#include <autoattach/lua_module.h>

#include <atomic>
#include <gumpp.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace luadebug::autoattach {
    struct ctx {
        fn_attach debuggerAttach;
        bool attach_process;

        std::mutex mtx;
        std::optional<lua_module> lua_module;
        std::atomic_bool wait_for_dll             = false;
        Gum::RefPtr<Gum::Interceptor> interceptor = Gum::Interceptor_obtain();

        static ctx* get() {
            static ctx obj;
            return &obj;
        }

        static attach_status attach_lua_vm(lua::state L) {
            return get()->debuggerAttach(L);
        }
    };
}