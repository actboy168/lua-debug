#pragma once

struct rlua_State;

namespace luadebug {
    int require_all(rlua_State* L);
}
