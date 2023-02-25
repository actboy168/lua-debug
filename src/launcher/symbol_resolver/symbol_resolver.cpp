#include <stdio.h>
#include <map>
#include <unordered_map>
#include <array>

#include <bee/utility/file_handle.h>
#include <bee/nonstd/filesystem.h>

#include "../common.hpp"
#include "symbol_resolver.h"
#include "../utility/string_helper.hpp"

namespace autoattach::symbol_resolver {
    std::unique_ptr<interface> create_gum_resolver(const RuntimeModule &module);

    std::unique_ptr<interface> symbol_resolver_factory_create(const RuntimeModule &module) {
        return create_gum_resolver(module);
    }

}