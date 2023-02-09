#include <stdio.h>
#include <map>
#include <unordered_map>
#include <array>
#include <fstream>
#include <streambuf>
#include <optional>
#include <algorithm>

#include <bee/utility/file_handle.h>
#include <bee/filesystem.h>

#include "../common.hpp"
#include "symbol_resolver.h"
#include "../utility/string_helper.hpp"
#include "signatures.hpp"

namespace autoattach::symbol_resolver {

    std::optional<signatures> check_signature() {
        const char *signature = getenv("LUA_DEBUG_SIGNATURE");
        if (signature) {
            return signatures(signature);
        }
        return std::nullopt;
    }

    std::unique_ptr<interface> create_dobby_resolver(const RuntimeModule &module);

    std::unique_ptr<interface> create_signature_resolver(const RuntimeModule &module, signatures&& data);

    std::unique_ptr<interface> symbol_resolver_factory_create(const RuntimeModule &module) {
        auto signatures = check_signature();
        if (signatures.has_value()) {
            return create_signature_resolver(module, std::move(signatures.value()));
        }
        return create_dobby_resolver(module);
    }

}