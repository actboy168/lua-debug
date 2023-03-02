#pragma once

#include "../common.hpp"
#include <memory>

namespace lua_delayload {
    struct resolver;
}

namespace luadebug::autoattach {
    struct vmhook {
        virtual ~vmhook() = default;

        virtual bool hook() = 0;

        virtual void unhook() = 0;

        virtual bool get_symbols(const lua_delayload::resolver& resolver) = 0;
    };

    vmhook *create_vmhook(lua_version v);
}