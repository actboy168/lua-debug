#if defined(_MSC_VER)
#include "rdebug_delayload.h"
#include <Windows.h>
#include <stdint.h>

namespace remotedebug {
class pe_reader {
public:
    pe_reader()
        : m_moudle(0)
    { }
    pe_reader(HMODULE module) 
        : m_moudle((uintptr_t)module) 
    { }
    void set_module(HMODULE module) {
        m_moudle = (uintptr_t)module;
    }
    HMODULE module() const {
        return (HMODULE)(m_moudle);
    }
    size_t module_size() const {
        return get_nt_headers()->OptionalHeader.SizeOfImage + get_nt_headers()->OptionalHeader.SizeOfHeaders;
    }
    uintptr_t rva_to_addr(uintptr_t rva) const {
        if (rva == 0) return 0;
        return m_moudle + rva;
    }
    PIMAGE_DOS_HEADER get_dos_header() const {
        return (PIMAGE_DOS_HEADER)(m_moudle);
    }
    PIMAGE_NT_HEADERS get_nt_headers() const {
        PIMAGE_DOS_HEADER dos_header = get_dos_header();
        return (PIMAGE_NT_HEADERS)((uintptr_t)(dos_header) + dos_header->e_lfanew);
    }
    uintptr_t get_directory_entry(uint32_t directory) const {
        PIMAGE_NT_HEADERS nt_headers = get_nt_headers();
        return rva_to_addr(nt_headers->OptionalHeader.DataDirectory[directory].VirtualAddress);
    }
    uint32_t get_directory_entry_size(uint32_t directory) const {
        PIMAGE_NT_HEADERS nt_headers = get_nt_headers();
        return nt_headers->OptionalHeader.DataDirectory[directory].Size;
    }
private:
    uintptr_t m_moudle;
};

static uintptr_t find_putenv() {
    HMODULE luadll = delayload::get_luadll();
    if (!luadll) {
        return 0;
    }
    pe_reader image(luadll);
    PIMAGE_IMPORT_DESCRIPTOR import = (PIMAGE_IMPORT_DESCRIPTOR)image.get_directory_entry(IMAGE_DIRECTORY_ENTRY_IMPORT);
    uint32_t size = image.get_directory_entry_size(IMAGE_DIRECTORY_ENTRY_IMPORT);
    if (import == NULL || size < sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
        return 0;
    }
    for (; import->FirstThunk; ++import) {
        PIMAGE_THUNK_DATA pitd = (PIMAGE_THUNK_DATA)image.rva_to_addr(import->OriginalFirstThunk);
        PIMAGE_THUNK_DATA pitd2 = (PIMAGE_THUNK_DATA)image.rva_to_addr(import->FirstThunk);
        for (;pitd->u1.Function; ++pitd, ++pitd2) {
            PIMAGE_IMPORT_BY_NAME pi_import_by_name = (PIMAGE_IMPORT_BY_NAME)(image.rva_to_addr(*(uintptr_t*)pitd));
            if (!IMAGE_SNAP_BY_ORDINAL(pitd->u1.Ordinal)) {
                const char* apiname = (const char*)pi_import_by_name->Name;
                if (0 == strcmp(apiname, "getenv") || 0 == strcmp(apiname, "_wgetenv")) {
                    HMODULE crt = GetModuleHandleA((const char*)image.rva_to_addr(import->Name));
                    if (crt) {
                        return (uintptr_t)GetProcAddress(crt, "_putenv");
                    }
                }
            }
        }
    }
    return 0;
}

void putenv(const char* envstr) {
    static auto lua_putenv = (int (__cdecl*) (const char*))find_putenv();
    if (lua_putenv) {
        lua_putenv(envstr);
    }
    else {
        ::_putenv(envstr);
    }
}

}
#endif
