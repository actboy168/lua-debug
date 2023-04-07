#pragma once
#include <autoattach/autoattach.h>
#include <autoattach/lua_module.h>
#include <autoattach/session.h>

#include <atomic>
#include <gumpp.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace luadebug::autoattach {
    struct ctx {
        std::mutex mtx;
        bool wait_dll = false;
        std::optional<struct lua_module> lua_module;
        std::optional<struct session> session;

        static ctx* get() {
            static ctx obj;
            return &obj;
        }
    };
}