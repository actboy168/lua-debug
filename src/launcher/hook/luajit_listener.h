#pragma once

#include <stdint.h>

namespace luadebug::autoattach {

    uintptr_t jit2state(void* ctx);
    uintptr_t global2state(void* ctx);

}
