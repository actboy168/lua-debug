#pragma once

#include <autoattach/luaversion.h>
#include <memory>

namespace luadebug::lua {
    struct resolver;
}

namespace luadebug::autoattach {
    struct vmhook {
        virtual ~vmhook() = default;

        virtual bool hook() = 0;

        virtual void unhook() = 0;

        virtual bool get_symbols(const lua::resolver& resolver) = 0;
    };

    vmhook *create_vmhook(lua_version v);
}