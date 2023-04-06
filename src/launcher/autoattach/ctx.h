#pragma once
#include <autoattach/autoattach.h>
#include <autoattach/lua_module.h>
#include <config/config.h>

#include <atomic>
#include <gumpp.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace luadebug::autoattach {
    struct ctx {
        std::mutex mtx;
        work_mode mode;
        std::optional<struct lua_module> lua_module;
        bool wait_dll = false;
        std::optional<config::Config> config;

        static ctx* get() {
            static ctx obj;
            return &obj;
        }
    };
}