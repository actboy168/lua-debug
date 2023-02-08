#include "symbol_resolver.h"
#include <dobby.h>

namespace autoattach::symbol_resolver {
    struct dobby_resolver : interface {
		dobby_resolver(const RuntimeModule& module):image_name{module.path}{}
        ~dobby_resolver() override;

        const char *image_name;

        void *getsymbol(const char *name) const override {
            return DobbySymbolResolver(image_name, name);
        }
    };
	std::unique_ptr<interface> create_dobby_resolver(const RuntimeModule& module) {
		return std::make_unique<dobby_resolver>(module);
	}
}