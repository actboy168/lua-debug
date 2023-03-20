#include <bee/nonstd/filesystem.h>

#include <gumpp.hpp>
#include <iostream>
#include <list>
#include <set>
#include <string>
#include <string_view>

#ifdef _WIN32

#else
#    include <dlfcn.h>
#endif

using namespace std::literals;
const char* module_name;
struct Pattern {
    std::string name;
    Gum::signature signature;
    size_t hit_offset;
};

int imports(std::string file_path, bool is_string, std::set<std::string>& imports_names) {
#ifdef _WIN32

#else
    auto handle = dlopen(file_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "dlopen " << file_path << " failed: " << dlerror() << std::endl;
        return 1;
    }
#endif

    if (is_string) {
        // check address is a string begin
        for (auto str : Gum::search_module_string(file_path.c_str(), "lua")) {
            std::string_view name(str);
            if (str[-1] != 0) {
                continue;
            }
            // check name has '_' and not has ' ' '.' '*' '/' '\\' ':' '<' '>' '|' '"' '?'
            if (name.find('_') == std::string_view::npos) {
                continue;
            }
            if (name.find_first_of(" .*/\\:<>|\"?") != std::string_view::npos) {
                continue;
            }
            if (name.find("lua") != 0) {
                continue;
            }

            imports_names.emplace(name);
        }
    }
    else {
        Gum::Process::module_enumerate_import(file_path.c_str(), [&](const Gum::ImportDetails& details) {
            auto name = std::string_view(details.name);
            if (name.find("lua") == 0) {
                imports_names.insert(details.name);
            }
            return true;
        });
    }
#ifndef NDEBUG
    for (const auto& name : imports_names) {
        std::cerr << name << std::endl;
    }
#endif
    return 0;
}
bool starts_with(std::string_view _this, std::string_view __s) noexcept {
    return _this.size() >= __s.size() &&
           _this.compare(0, __s.size(), __s) ==
               0;
}
constexpr auto insn_max_limit = 100;
constexpr auto insn_min_limit = 4;

bool compiler_signature(const char* name, void* address, std::list<Pattern>& patterns, int limit = insn_min_limit) {
    auto signature                  = Gum::get_function_signature(address, limit);
    auto& pattern                   = signature.pattern;
    std::vector<void*> find_address = Gum::search_module_function(module_name, pattern.c_str());
    if (find_address.size() > 1 && limit <= insn_max_limit) {
        return compiler_signature(name, address, patterns, limit + 1);
    }
    if (find_address.size() != 1) {
        std::cerr << name << " address:" << address << " pattern:" << pattern << std::endl;
        if (find_address.empty()) {
            for (size_t i = 0; i < pattern.size(); i += 9) {
                if (i + 8 < pattern.size()) {
                    pattern.insert(i + 8, "\n");
                }
            }
            std::cerr << name << ": invalid pattern[can't search pattern]\n"
                      << pattern << std::endl;
            exit(1);
        }
        else {
            if (find_address.size() > 8) {
                std::cerr << name << ": invalid pattern[too many matched]:" << find_address.size()
                          << " hited" << std::endl;
            }
            else {
                for (auto addr : find_address) {
                    std::cerr << "\taddress:" << addr << std::endl;
                }
            }
        }
    }
    bool isvalid  = false;
    size_t offset = 0;
    for (auto addr : find_address) {
        if ((void*)((uint8_t*)addr + signature.offset) == address) {
            isvalid = true;
            break;
        }
        offset++;
    }

    if (!isvalid) {
        std::cerr << name << ": invalid pattern[address not match]:" << address << std::endl;
        return false;
    }
    else {
        patterns.emplace_back(Pattern { name, signature, offset });
        return true;
    }
}

int main(int narg, const char* argvs[]) {
    if (narg != 5) {
        std::cerr << "Usage: "
                  << "signautre import_file is_string target_file is_export"
                  << std::endl;
        return 1;
    }

    Gum::runtime_init();
    auto import_file = std::filesystem::absolute(argvs[1]).generic_string();
    auto is_string   = argvs[2] == "true"sv ? true : false;
    std::set<std::string> imports_names;
    if (auto ec = imports(import_file, is_string, imports_names); ec != 0) {
        return ec;
    }
    auto target_path = std::filesystem::absolute(argvs[3]).generic_string();
    auto is_export   = argvs[4] == "true"sv ? true : false;
    std::string error;
    module_name = target_path.c_str();
    std::cerr << "signature:" << module_name << std::endl;

    if (!Gum::Process::module_load(module_name, &error)) {
        std::cerr << "Load " << module_name << " failed: " << error << std::endl;
        return 1;
    }

    // imports_names remove lua_ident luaJIT_version_*
    {
        auto iter = imports_names.begin();
        while (iter != imports_names.end()) {
            const auto& name = *iter;
            if (name == "lua_ident" || starts_with(name, "luaJIT_version_")) {
                iter = imports_names.erase(iter);
            }
            else {
                ++iter;
            }
        }
    }

    std::list<Pattern> patterns;
    if (is_export) {
        Gum::Process::module_enumerate_export(module_name, [&](const Gum::ExportDetails& details) {
            if (imports_names.find(details.name) != imports_names.end()) {
                if (compiler_signature(details.name, details.address, patterns)) {
                    imports_names.erase(details.name);
                }
            }
            return true;
        });
    }
    else {
        Gum::Process::module_enumerate_symbols(module_name, [&](const Gum::SymbolDetails& details) {
            if (imports_names.find(details.name) != imports_names.end()) {
                if (compiler_signature(details.name, details.address, patterns)) {
                    imports_names.erase(details.name);
                }
            }
            return true;
        });
    }

    for (const auto& pattern : patterns) {
        std::cout << pattern.name << ":" << pattern.signature.pattern << "(" << (intptr_t)pattern.signature.offset << ")"
                  << "@" << pattern.hit_offset << std::endl;
    }
    for (auto name : imports_names) {
        std::cerr << "can't find signature: " << name << std::endl;
    }

    return 0;
}
