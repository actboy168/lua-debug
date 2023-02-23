
#include <frida-gum.h>

#include "gumpp.hpp"
#include "runtime.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#include <pathcch.h>
#pragma comment(lib, "dbghelp.lib")
#endif

#include <string_view>

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
        Runtime::ref();
        gum_process_enumerate_modules([](const GumModuleDetails * details, void* p)->gboolean{
            const auto func = (FoundModuleFunc*)p;
            ModuleDetailsImpl impl{details};
            return (*func)(impl);
        }, (void*)&func);
        Runtime::unref();
    }

    void* Process::module_find_symbol_by_name(const char* module_name, const char* symbol_name) {
#ifndef _WIN32
        Runtime::ref();
        auto ptr = GSIZE_TO_POINTER(gum_module_find_symbol_by_name(module_name, symbol_name));
        Runtime::unref();
        return ptr;
#else
        RefPtr<PtrArray> functions = RefPtr<PtrArray> (find_matching_functions_array (symbol_name));
        for (size_t i = 0; i < functions->length(); i++)
        {
            GumDebugSymbolDetails details;
            if (gum_symbol_details_from_address(functions->nth(i), &details)){
                //TODO: should be module_name
                if (details.file_name == std::string_view(module_name)) {
                    return GSIZE_TO_POINTER(details.address);
                }
            }
        }
        return nullptr;
#endif
    }
}