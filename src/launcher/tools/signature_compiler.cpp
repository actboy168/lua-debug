#include <gumpp.hpp>
#include <iostream>
#include <list>
#include <string>
#include <string_view>
#include <bee/nonstd/filesystem.h>
#include <set>

#ifdef _WIN32

#else
#    include <dlfcn.h>
#endif

using namespace std::literals;
const char* module_name;
struct Pattern {
    std::string name;
    std::string pattern;
    size_t scan_count;
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
    for (const auto& name : imports_names) {
        std::cerr << name << std::endl;
    }
    return 0;
}
bool starts_with(std::string_view _this, std::string_view __s) _NOEXCEPT {
    return _this.size() >= __s.size() &&
           _this.compare(0, __s.size(), __s) ==
               0;
}

bool compiler_pattern(const char* name, void* address, std::list<Pattern>& patterns) {
    auto pattern = Gum::to_signature_pattern(address, 255);
    std::vector<void*> find_address = Gum::search_module_function(module_name, pattern.c_str());
    if (find_address.size() != 1) {
        std::cerr << name << " address:" << address << " pattern:" << pattern << std::endl;
        if (find_address.empty()) {
            std::cerr << name << ": invalid pattern[can't search pattern]" << std::endl;
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
    bool isvalid = false;
    for (auto addr : find_address) {
        if (addr == address) {
            isvalid = true;
            break;
        }
    }

    if (!isvalid) {
        std::cerr << name << ": invalid pattern[address not match]:" << address << std::endl;
        return false;
    }
    else {
        patterns.emplace_back(Pattern { name, pattern, find_address.size() });
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
    auto is_string = argvs[2] == "true"sv ? true : false;
    std::set<std::string> imports_names;
    if (auto ec = imports(import_file, is_string, imports_names); ec != 0) {
        return ec;
    }
    auto target_path = std::filesystem::absolute(argvs[3]).generic_string();
    auto is_export = argvs[4] == "true"sv ? true : false;
    std::string error;
    module_name = target_path.c_str();
    std::cerr << "signature:" << module_name << std::endl;

    if (!Gum::Process::module_load(module_name, &error)) {
        std::cerr << "Load " << module_name << " failed: " << error << std::endl;
        return 1;
    }

    std::list<Pattern> patterns;
    if (is_export) {
        Gum::Process::module_enumerate_export(module_name, [&](const Gum::ExportDetails& details) {
            if (imports_names.find(details.name) != imports_names.end())
                compiler_pattern(details.name, details.address, patterns);
            return true;
        });
    }
    else {
        Gum::Process::module_enumerate_symbols(module_name, [&](const Gum::SymbolDetails& details) {
            if (imports_names.find(details.name) != imports_names.end())
                compiler_pattern(details.name, details.address, patterns);
            return true;
        });
    }

    for (const auto& pattern : patterns) {
        std::cout << pattern.name << ":" << pattern.pattern << "-" << pattern.scan_count << std::endl;
    }
    return 0;
}
