#pragma once

#include "rlua.h"

namespace remotedebug {
    const rluaL_Reg* get_cmodule();
    int require_all(rlua_State* L);
}
