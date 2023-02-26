#include <string>
#include <cstdio>
#include <memory>

#include <unistd.h>

#include <bee/utility/path_helper.h>

#include "common.h"
#include "ipc.h"

namespace lua_debug_common {
fs::path get_root_path() {
    auto r =  bee::path_helper::dll_path();
    if (!r)
        return {};
    auto root = r.value().parent_path().parent_path();
#ifdef _WIN32
    return root.parent_path();
#else
    return root;
#endif
}

static std::string ipc_file_name(const std::string_view& name) {
    auto root = get_root_path();
    if (root.empty())
        return {};
    return (root / "tmp"/ "ipc_").string() + std::to_string(getpid()) + "_" + std::string(name);
}

static std::shared_ptr<FILE> init_file_handler(const std::string_view& name){
    if (name.empty())
        return {};
    return {fopen(name.data(), "a+"), &fclose};
}

void log_message(const char * fmt, ...) {
    static auto handler = init_file_handler((get_root_path() / "worker.log").generic_string());
    va_list ap;
    va_start(ap, fmt);
    if (!handler)
        vfprintf(stdout, fmt, ap);
    else {
#ifndef NDEBUG
        vfprintf(stderr, fmt, ap);
#endif
        vfprintf(handler.get(), fmt, ap);
    }
    va_end(ap);
}
constexpr auto failed_end_file_key = "__endfile__\n";
void send_failed_message(const char *fmt, ...){
    auto handler = init_file_handler(ipc_file_name("failed"));
    va_list ap;
    va_start(ap, fmt);
    if (!handler)
    {
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, failed_end_file_key);
    }   
    else
    {
        vfprintf(handler.get(), fmt, ap);
        fprintf(handler.get(), failed_end_file_key);
    }    
    log_message(fmt, ap);
    va_end(ap);
}
}