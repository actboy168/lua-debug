#pragma once

#include "../common.hpp"
#include <memory>

#ifdef interface
#undef interface
#endif

namespace autoattach {
	namespace symbol_resolver {
		struct interface;
	}
    struct vmhook {
        virtual ~vmhook() = default;

        virtual bool hook() = 0;

        virtual void unhook() = 0;

        virtual bool get_symbols(const std::unique_ptr<symbol_resolver::interface> &resolver) = 0;
    };

    vmhook *create_vmhook(lua_version v);
}