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
        const char *signature = nullptr;//getenv("LUA_DEBUG_SIGNATURE");
        if (signature) {
            std::string data;
            if (!fs::exists(signature)) {
                data = signature;
            } else {
                //this is a file name
                std::ifstream file(signature, std::ios_base::binary | std::ios_base::ate);
                auto length = file.tellg();
                data.reserve(length);
                file.seekg(std::ios_base::beg);

                file >> data;
                // skip \t
                data.erase(std::remove_if(data.begin(), data.end(), [](char c) { return c == '\t'; }));
            }
            return signatures(std::move(data));
        }
        return std::nullopt;
    }

    std::unique_ptr<interface> create_dobby_resolver(const RuntimeModule &module);

    std::unique_ptr<interface> create_signature_resolver(const RuntimeModule &module, const signatures &data);

    std::unique_ptr<interface> symbol_resolver_factory_create(const RuntimeModule &module) {
        const auto &signatures = check_signature();
        if (signatures.has_value()) {
            return create_signature_resolver(module, signatures.value());
        }
        return create_dobby_resolver(module);
    }

}