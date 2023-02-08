#pragma once

#include "../common.hpp"
#include <memory>

namespace autoattach::symbol_resolver {
    struct interface {
        virtual ~interface() = default;

        virtual void *getsymbol(const char *name) const = 0;
    };

    std::unique_ptr <interface> symbol_resolver_factory_create(const RuntimeModule &module);
}