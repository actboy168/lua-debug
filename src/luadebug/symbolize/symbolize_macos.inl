#include <dlfcn.h>
#include <symbolize/symbolize.h>
#include <unistd.h>

#if defined(__GNUC__)
#    include <cxxabi.h>
#endif

#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/format.h>
#include <bee/subprocess.h>

#include <memory>

namespace luadebug {
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

    static std::optional<std::string> get_function_atos(void* ptr) {
        struct AtosInfo {
            std::string pid;
            AtosInfo() {
                pid = std::format("{}", getpid());
            }
        };
        static AtosInfo atos_info;
        auto ptr_s         = std::format("{}", ptr);
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

    std::optional<std::string> symbolize(void* ptr) {
        if (!ptr) {
            return std::nullopt;
        }
        Dl_info info = {};
        if (dladdr(ptr, &info) == 0) {
            return std::nullopt;
        }
        if (ptr > info.dli_fbase) {
            void* calc_address                  = info.dli_saddr == ptr ? info.dli_saddr : ptr;
            std::optional<std::string> funcinfo = get_function_atos(calc_address);
            if (funcinfo.has_value()) {
                return funcinfo;
            }
        }
        if (info.dli_saddr != ptr) {
            return std::nullopt;
        }
        std::string filename = fs::path(info.dli_fname).filename();
        std::string realname = demangle_name(info.dli_sname);
        auto addr            = info.dli_saddr;
        return std::format("`{}`{} {}", filename, realname, addr);
    }
}
