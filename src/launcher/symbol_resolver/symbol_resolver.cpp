#include <stdio.h>
#include <map>
#include <unordered_map>
#include <array>

#include <bee/utility/file_handle.h>
#include <bee/nonstd/filesystem.h>

#include "../common.hpp"
#include "symbol_resolver.h"
#include "../utility/string_helper.hpp"
#include "signatures.hpp"

namespace autoattach::symbol_resolver {
    std::unique_ptr<interface> create_dobby_resolver(const RuntimeModule &module);

    std::unique_ptr<interface> create_signature_resolver(const RuntimeModule &module, signatures&& data);

    std::unique_ptr<interface> symbol_resolver_factory_create(const RuntimeModule &module) {
        const char *signature = getenv("LUA_DEBUG_SIGNATURE");
        if (signature) {
            return create_signature_resolver(module, signatures(signature));
        }
        return create_dobby_resolver(module);
    }

}