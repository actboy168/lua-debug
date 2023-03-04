#pragma once

#include <stdint.h>

namespace luadebug::autoattach {
    struct vm_watcher {
        using value_t = uintptr_t;
        static_assert(sizeof(value_t) == sizeof(void*));
        
        virtual ~vm_watcher() = default;
        virtual void watch_entry(value_t L) = 0;
    };
}