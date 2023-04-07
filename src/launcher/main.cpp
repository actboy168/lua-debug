#include <autoattach/autoattach.h>
#include <autoattach/ctx.h>
#include <bee/nonstd/filesystem.h>
#include <bee/utility/path_helper.h>
#include <util/log.h>

#ifndef _WIN32
#    define DLLEXPORT __attribute__((visibility("default")))
#    define DLLEXPORT_DECLARATION
#else
#    define DLLEXPORT __declspec(dllexport)
#    define DLLEXPORT_DECLARATION __cdecl
#endif

#include <gumpp.hpp>
#include <string>
#include <string_view>

namespace luadebug::autoattach {
    static std::string readfile(const fs::path &filename) {
#ifdef _WIN32
        FILE *f = _wfopen(filename.c_str(), L"rb");
#else
        FILE *f = fopen(filename.c_str(), "rb");
#endif
        if (!f) {
            return std::string();
        }
        fseek(f, 0, SEEK_END);
        long length = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::string tmp;
        tmp.resize(length);
        fread(tmp.data(), 1, length, f);
        fclose(f);
        return tmp;
    }

    attach_status attach_lua(lua::state L, lua_module &module) {
        log::info("attach lua vm entry");
        auto r = bee::path_helper::dll_path();
        if (!r) {
            log::fatal("dll_path error: {}", r.error());
            return attach_status::fatal;
        }
        auto root = r.value().parent_path().parent_path();
        auto buf  = readfile(root / "script" / "attach.lua");
        if (lua::loadbuffer(L, buf.data(), buf.size(), "=(attach.lua)")) {
            log::fatal("load attach.lua error: {}", lua::tostring(L, -1));
            lua::pop(L, 1);
            return attach_status::fatal;
        }
        lua::call<lua_pushstring>(L, root.generic_u8string().c_str());
        lua::call<lua_pushstring>(L, std::to_string(Gum::Process::get_id()).c_str());
        lua::call<lua_pushstring>(L, module.debugger_path.c_str());

        if (lua::pcall(L, 3, 1, 0)) {
            /*
                这里失败无法调用log::fatal，因为无法知道调试器已经加载到哪一步才失败的。
                所以调试器不应该把错误抛到这里。
            */
            log::error("{}", lua::tostring(L, -1));
            lua::pop(L, 1);
            return attach_status::wait;
        }
        using namespace std::string_view_literals;
        bool ok = lua::tostring(L, -1) == "ok"sv;
        lua::pop(L, 1);
        return ok ? attach_status::success : attach_status::wait;
    }
}

extern "C" {
DLLEXPORT void DLLEXPORT_DECLARATION launch() {
    luadebug::autoattach::initialize(luadebug::autoattach::work_mode::launch);
}

DLLEXPORT void DLLEXPORT_DECLARATION attach() {
    luadebug::autoattach::initialize(luadebug::autoattach::work_mode::attach);
}
}
