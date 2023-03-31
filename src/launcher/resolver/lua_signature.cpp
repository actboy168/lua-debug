#include <config/config.h>
#include <resolver/lua_signature.h>
#include <util/log.h>

#include <gumpp.hpp>
namespace luadebug::autoattach {
    intptr_t signature::find(const char* module_name) const {
        auto all_address = Gum::search_module_function(module_name, pattern.c_str());
        if (all_address.empty())
            return 0;

        void* find_address = nullptr;
        if (all_address.size() == 1)
            find_address = all_address[0];
        else {
            Gum::MemoryRange range;
            Gum::Process::enumerate_modules([&](const Gum::ModuleDetails& details) {
                if (details.name() == std::string_view(module_name) || details.path() == std::string_view(module_name)) {
                    range = details.range();
                    return false;
                }
                return true;
            });

            range.size -= end_offset;
            range.base_address = (void*)((uint8_t*)range.base_address + start_offset);

            // 限定匹配地址范围
            auto iter = std::remove_if(all_address.begin(), all_address.end(), [&](void* address) {
                return !range.contains(address);
            });
            all_address.erase(iter, all_address.end());

            auto hit_index = all_address.size() > hit_offset ? hit_offset : 0;
            find_address   = all_address[hit_index];
        }
        if (find_address != 0)
            return (uintptr_t)((uint8_t*)find_address + pattern_offset);
        return 0;
    }

    intptr_t signature_resolver::find(std::string_view name) const {
        auto addr = resolver->find(name);
        if (addr)
            return addr;

        if (auto it = config->signatures.find(std::string(name)); it != config->signatures.end()) {
            addr = it->second.find(module_name.data());
            if (addr)
                return addr;
        }
        log::info("reserve_resolver find: {}", name);
        return reserve_resolver->find(name);
    }
}