#include <autoattach/lua_version.h>

namespace luadebug::autoattach {
    const char* lua_version_to_string(lua_version v) {
        switch (v) {
        case lua_version::lua51:
            return "lua51";
        case lua_version::lua52:
            return "lua52";
        case lua_version::lua53:
            return "lua53";
        case lua_version::lua54:
            return "lua54";
        case lua_version::luajit:
            return "luajit";
        case lua_version::latest:
            return "lua-latest";
        default:
            return "unknown";
        }
    }
    lua_version lua_version_from_string(const std::string_view& v) {
        if (v == "jit" || v == "luajit")
            return lua_version::luajit;
        else if (v == "5.1" || v == "lua51")
            return lua_version::lua51;
        else if (v == "5.2" || v == "lua52")
            return lua_version::lua52;
        else if (v == "5.3" || v == "lua53")
            return lua_version::lua53;
        else if (v == "5.4" || v == "lua54")
            return lua_version::lua54;
        else if (v == "latest" || v == "lua-latest")
            return lua_version::latest;
        return lua_version::unknown;
    }
}