#include "symbol_resolver.h"
#include "../utility/string_helper.hpp"
#include "gumpp.hpp"

namespace autoattach::symbol_resolver {
    struct gum_resolver : interface {
		gum_resolver(const RuntimeModule& module):image_name{module.name}{}
        ~gum_resolver() override = default;

        const char *image_name;

        void *getsymbol(const char *name) const override {
            auto keys = strings::spilt_string(name, '.');
            auto key = keys.back().data();
            auto ptr = Gum::Process::module_find_export_by_name(image_name, key);
            if (!ptr)
                ptr = Gum::Process::module_find_symbol_by_name(image_name, key);
            LOG(std::format("gum resolver symbol {}:{}", name, ptr).c_str());
            return ptr;
        }
    };
	std::unique_ptr<interface> create_gum_resolver(const RuntimeModule& module) {
		return std::make_unique<gum_resolver>(module);
	}
}