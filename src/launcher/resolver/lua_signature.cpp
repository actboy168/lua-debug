#include <resolver/lua_signature.h>

#include <gumpp.hpp>

namespace luadebug::autoattach {
    intptr_t signture::find(const char* module_name) const {
        auto all_address = Gum::search_module_function(module_name, pattern.c_str());
        if (all_address.empty())
            return 0;

        if (all_address.size() == 1)
            return (uintptr_t)all_address[0];

        Gum::MemoryRange range;
        Gum::Process::enumerate_modules([&](const Gum::ModuleDetails& details) {
            if (details.name() == std::string_view(module_name) || details.path() == std::string_view(module_name)) {
                range = details.range();
                return false;
            }
            return true;
        });

        range.size -= end_offset + start_offset;
        range.base_address = (void*)((uintptr_t)range.base_address + start_offset);

        if (hit_offset != 0 && all_address.size() > hit_offset) {
            return (uintptr_t)all_address[hit_offset];
        }

        for (auto address: all_address) {
            if (range.contains(address)) {
                return (uintptr_t)address;
            }
        }

        return (uintptr_t)all_address[0];
    }
}