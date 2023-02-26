#pragma once

namespace lua_debug_common{
void log_message(const char * fmt, ...);
void send_failed_message(const char *fmt, ...);

#define LUA_DEBUG_LOG_FMT(prefix, fmt) "[lua-debug]" "[" #prefix "]" fmt "\n"
#define LUA_DEBUG_LOG(prefix, fmt, ...) ::lua_debug_common::log_message(LUA_DEBUG_LOG_FMT(prefix, fmt), ##__VA_ARGS__)
#define LUA_DEBUG_FAIL_LOG(prefix, fmt, ...) ::lua_debug_common::send_failed_message(fmt "\n", ##__VA_ARGS__)
}