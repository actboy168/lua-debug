
#include <frida-gum.h>

#include "gumpp.hpp"

namespace Gum {
    struct ModuleDetailsImpl : ModuleDetails {
        const GumModuleDetails * details;
        ModuleDetailsImpl(const GumModuleDetails * d):details{d}{}
        virtual ~ModuleDetailsImpl() = default;
        virtual const char* name()  const{
            return details->name;
        }
        virtual MemoryRange range()  const{
            return MemoryRange{GSIZE_TO_POINTER(details->range->base_address), details->range->size};
        }
        virtual const char* path() const {
            return details->path;
        }
    };

    void Process::enumerate_modules(const FoundModuleFunc& func) {
        gum_process_enumerate_modules([](const GumModuleDetails * details, void* p)->gboolean{
            const auto& func = *(FoundModuleFunc*)p;
            ModuleDetailsImpl impl{details};
            return func(impl);
        }, (void*)&func);
    }

    void* Process::module_find_symbol_by_name(const char* module_name, const char* symbol_name) {
        return GSIZE_TO_POINTER(gum_module_find_symbol_by_name(module_name, symbol_name));
    }
}