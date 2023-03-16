#include "rdebug_cfunctioninfo.h"

#if defined(_WIN32)
#include <windows.h>
#include <DbgHelp.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#if defined(__GNUC__)
#include <cxxabi.h>
#endif

#include <bee/subprocess.h>
#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/format.h>
#include <memory>

namespace luadebug {
#if defined(_WIN32)
    struct SymHandler {
        HANDLE hProcess = NULL;
        ~SymHandler() {
            SymCleanup(hProcess);
            hProcess = NULL;
        }
        operator bool() const noexcept {
            return hProcess != NULL;
        }
    };
    inline std::string searchpath(bool SymBuildPath, bool SymUseSymSrv) {
        // Build the sym-path:
        if (SymBuildPath) {
            std::string searchpath;
            searchpath.reserve(4096);
            searchpath.append(".;");
            std::error_code ec;
            auto current_path = std::filesystem::current_path(ec);
            if (!ec) {
                searchpath.append(current_path.string().c_str());
                searchpath += ';';
            }
            const size_t nTempLen = 1024;
            char szTemp[nTempLen];

            // Now add the path for the main-module:
            if (GetModuleFileNameA(NULL, szTemp, nTempLen) > 0) {
                std::filesystem::path path(szTemp);
                searchpath.append(path.parent_path().string());
                searchpath += ';';
            }
            if (GetEnvironmentVariableA("_NT_SYMBOL_PATH", szTemp, nTempLen) > 0) {
                szTemp[nTempLen - 1] = 0;
                searchpath.append(szTemp);
                searchpath += ';';
            }
            if (GetEnvironmentVariableA("_NT_ALTERNATE_SYMBOL_PATH", szTemp, nTempLen) > 0) {
                szTemp[nTempLen - 1] = 0;
                searchpath.append(szTemp);
                searchpath += ';';
            }
            if (GetEnvironmentVariableA("SYSTEMROOT", szTemp, nTempLen) > 0) {
                szTemp[nTempLen - 1] = 0;
                searchpath.append(szTemp);
                searchpath += ';';
                searchpath.append(szTemp);
                searchpath.append("\\system32;");
            }

            if (SymUseSymSrv) {
                if (GetEnvironmentVariableA("SYSTEMDRIVE", szTemp, nTempLen) > 0) {
                    szTemp[nTempLen - 1] = 0;
                    searchpath.append("SRV*");
                    searchpath.append(szTemp);
                    searchpath.append("\\websymbols*https://msdl.microsoft.com/download/symbols;");
                }
                else
                    searchpath.append("SRV*c:\\websymbols*https://msdl.microsoft.com/download/symbols;");
            }
            return searchpath;
        } // if SymBuildPath
        return {};
    }

    inline HANDLE createSymHandler(bool SymBuildPath, bool SymUseSymSrv) {
        HANDLE proc = GetCurrentProcess();
        auto path = searchpath(SymBuildPath, SymUseSymSrv);
        if (!SymInitialize(proc, path.c_str(), TRUE)) {
            if (GetLastError() != 87) {
                return nullptr;
            }
        }
        DWORD symOptions = SymGetOptions(); // SymGetOptions
        symOptions |= SYMOPT_LOAD_LINES;
        symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS;
        symOptions = SymSetOptions(symOptions);
        return proc;
    }

    inline SymHandler& GetSymHandler() {
        static SymHandler handler { createSymHandler(true, true) };
        return handler;
    }

    struct SymbolFileInfo {
        std::string name;
        uint32_t lineno = 0;
    };

    struct Symbol {
        std::string module_name;
        std::string function_name;
        std::optional<SymbolFileInfo> file;
    };

    inline std::optional<Symbol> Addr2Symbol(void* pObject) {
        static constexpr size_t max_sym_name =
#ifdef MAX_SYM_NAME
            MAX_SYM_NAME;
#else
            2000;
#endif // DEBUG
        struct MY_SYMBOL_INFO : SYMBOL_INFO {
            char name_buffer[MAX_SYM_NAME];
        };

        auto& handler = GetSymHandler();
        if (!handler) {
            return std::nullopt;
        }
        using PTR_T =
#ifdef _WIN64
            DWORD64;
#else
            DWORD;
#endif
        PTR_T dwAddress = PTR_T(pObject);
        DWORD64 dwDisplacement = 0;

        MY_SYMBOL_INFO sym = {};
        sym.SizeOfStruct = sizeof(SYMBOL_INFO);
        sym.MaxNameLen = max_sym_name;
        if (!SymFromAddr(handler.hProcess, dwAddress, &dwDisplacement, &sym)) {
            return std::nullopt;
        }
        if (sym.Flags & SYMFLAG_PUBLIC_CODE) {
#if defined(_M_AMD64)
            uint8_t OP = *(uint8_t*)dwAddress;
            if (OP == 0xe9) {
                int32_t offset = *(int32_t*)((char*)dwAddress + 1);
                return Addr2Symbol((void*)(dwAddress + 5 + offset));
            }
#else
            //TODO ARM64/ARM64EC
#endif // _M_AMD64
        }
        Symbol sb;
        {
            char buffer[256];
            if (UnDecorateSymbolName(sym.Name, buffer, 256, UNDNAME_COMPLETE) != 0)
                sb.function_name = buffer;
            else
                sb.function_name = sym.Name;
        }
        IMAGEHLP_MODULE md = {};
        md.SizeOfStruct = sizeof(IMAGEHLP_MODULE);
        if (SymGetModuleInfo(handler.hProcess, dwAddress, &md)) {
#if defined(_WIN64)
            if (md.LineNumbers) {
#endif
                IMAGEHLP_LINE line = {};
                line.SizeOfStruct = sizeof(IMAGEHLP_LINE);
                DWORD lineDisplacement = 0;
                if (SymGetLineFromAddr(handler.hProcess, dwAddress, &lineDisplacement, &line)) {
                    sb.file = { line.FileName, line.LineNumber };
                }
#if defined(_WIN64)
            }
#endif
            sb.module_name = md.ModuleName;
        }

        // Object name output
        return std::move(sb);
    };
#else
    static std::string demangle_name(const char* name) {
#if defined(__GNUC__)
        int status = 0;
        std::unique_ptr<char, decltype(&free)> realname(abi::__cxa_demangle(name, 0, 0, &status), free);
        return realname.get();
#endif
        return name;
    }
    static inline std::optional<std::string> shellcommand(const char** argp) {
        int pdes[2] = {};
        if (::pipe(pdes) < 0) {
            return std::nullopt;
        }
        auto pipe = bee::subprocess::pipe::open();
        if (!pipe) {
            return std::nullopt;
        }
        FILE* f = pipe.open_read();
        if (!f) {
            return std::nullopt;
        }
        bee::subprocess::spawn spawn;
        bee::subprocess::args_t args;
        for (auto p = argp; *p; ++p) {
            args.push(*p);
        }
        spawn.redirect(bee::subprocess::stdio::eOutput, pipe.wr);
        if (!spawn.exec(args, nullptr)) {
            return std::nullopt;
        }
        std::string res;
        char data[32];
        while (!::feof(f)) {
            if (::fgets(data, sizeof(data), f)) {
                res += data;
            }
            else {
                break;
            }
        }
        fclose(f);
        bee::subprocess::process process(spawn);
        process.wait();
        while (!res.empty() && (res[res.size() - 1] == '\n' || res[res.size() - 1] == '\r')) {
            res.erase(res.size() - 1);
        }
        return res;
    }

#if defined(__APPLE__)
    static std::optional<std::string> get_function_atos(void* ptr) {
        struct AtosInfo {
            std::string pid;
            AtosInfo() {
                pid = std::format("{}", getpid());
            }
        };
        static AtosInfo atos_info;
        auto ptr_s = std::format("{}", ptr);
        const char* args[] = {
            "/usr/bin/atos",
            "-p",
            atos_info.pid.c_str(),
            ptr_s.c_str(),
            nullptr,
        };
        if (auto funcinfo = shellcommand(args)) {
            if (!((*funcinfo)[0] == '0' && (*funcinfo)[1] == 'x')) {
                return funcinfo;
            }
        }
        return std::nullopt;
    }
#endif

#if defined(__linux__)
    static std::optional<std::string> get_function_addr2line(const char* fname, intptr_t offset) {
        auto offset_x = std::format("{:#x}", offset);
        const char* args[] = {
            "/usr/bin/addr2line",
            "-e",
            fname,
            "-fpCsi",
            offset_x.c_str(),
            nullptr,
        };
        auto res = shellcommand(args);
        if (!res)
            return std::nullopt;
        auto funcinfo = *res;
        if (!funcinfo.empty() && funcinfo[0] != '?' && funcinfo[1] != '?') {
            return funcinfo;
        }
        return std::nullopt;
    }
#endif
#endif // _WIN32

    std::optional<std::string> get_functioninfo(void* ptr) {
        if (!ptr) {
            return std::nullopt;
        }
#if defined(_WIN32)
        auto sym = Addr2Symbol(ptr);
        if (sym) {
            if (sym->file) {
                return std::format("{}!{} ({}:{})", sym->module_name, sym->function_name, sym->file->name, sym->file->lineno);
            }
            else {
                return std::format("{}!{}", sym->module_name, sym->function_name);
            }
        }
        return std::nullopt;
#else
        Dl_info info = {};
        if (dladdr(ptr, &info) == 0) {
            return std::nullopt;
        }

        if (ptr > info.dli_fbase) {
            void* calc_address = info.dli_saddr == ptr ? info.dli_saddr : ptr;
            std::optional<std::string> funcinfo
#if defined(__APPLE__)
                = get_function_atos(calc_address);
#elif defined(__linux__)
                = get_function_addr2line(info.dli_fname, (intptr_t)calc_address - (intptr_t)info.dli_fbase);
#else
                ;
#endif
            if (funcinfo.has_value()) {
                return funcinfo;
            }
        }

        if (info.dli_saddr != ptr) {
            return std::nullopt;
        }

        std::string filename = fs::path(info.dli_fname).filename();
        std::string realname = demangle_name(info.dli_sname);
        auto addr = info.dli_saddr;
        return std::format("`{}`{} {}", filename, realname, addr);
#endif
    }
}
