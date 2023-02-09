#include <dobby.h>
#include "symbol_resolver.h"
#include "../utility/string_helper.hpp"

namespace autoattach::symbol_resolver {
    struct dobby_resolver : interface {
		dobby_resolver(const RuntimeModule& module):image_name{module.path}{}
        ~dobby_resolver() override = default;

        const char *image_name;

        void *getsymbol(const char *name) const override {
			auto keys = strings::spilt_string(name, '.');
            return DobbySymbolResolver(image_name, keys.back().data());
        }
    };
	std::unique_ptr<interface> create_dobby_resolver(const RuntimeModule& module) {
		return std::make_unique<dobby_resolver>(module);
	}
}