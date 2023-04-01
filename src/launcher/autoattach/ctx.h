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
        bool attach_mode;

        std::mutex mtx;
        std::optional<struct lua_module> lua_module;

        static ctx* get() {
            static ctx obj;
            return &obj;
        }
    };
}