#include "autoattach.h"
#include <mutex>
#include <stack>
#include <set>
#include <vector>
#include "../remotedebug/rdebug_delayload.h"
#ifdef _WIN32
#include <intrin.h>
#include <Windows.h>
#include <winternl.h>
#else
#include <dlfcn.h>
#endif
#include <string>
#include <string_view>
#include <memory>
#include <charconv>
#include <filesystem>
#include <cstring>

#include "common.hpp"
#include "hook/hook_common.h"
#include "lua_resolver.h"
#include <gumpp.hpp>
namespace luadebug::autoattach {
	std::mutex lockLoadDll;
	fn_attach debuggerAttach;
	bool      attachProcess = false;

#ifdef _WIN32
	FARPROC luaapi(const char* name) {
        return nullptr;
	}
#endif

	lua_version get_lua_version_from_ident(const char* lua_ident){
		auto id = std::string_view(lua_ident);
        using namespace std::string_view_literals;
        constexpr auto key = "$Lua"sv;
        if (id.substr(0, key.length()) == key) {
            if (id[key.length()] == ':') {
                //$Lua: Lua 5.1.*
                return lua_version::lua51;
            }
            constexpr auto key = "$LuaVersion: Lua 5."sv;
            id = id.substr(key.length());
            int patch;
            auto res = std::from_chars(id.data(), id.data() + 3, patch, 10);
            if (res.ec != std::errc()) {
                return lua_version::unknown;
            }
            return (lua_version) ((int) lua_version::luajit + patch);
        }
        return lua_version::unknown;
	}
    lua_version get_lua_version(const char *path) {
        /*
            luaJIT_version_2_1_0_beta3
            luaJIT_version_2_1_0_beta2
            luaJIT_version_2_1_0_beta1
            luaJIT_version_2_1_0_alpha
        */
        if (!Gum::SymbolUtil::find_matching_functions("luaJIT_version_2_1_0*", true).empty()){
            return lua_version::luajit;
        }
		auto p = Gum::Process::module_find_symbol_by_name(path, "lua_ident");;
        const char *lua_ident = (const char *) p;
        if (!lua_ident)
            return lua_version::unknown;
		return get_lua_version_from_ident(lua_ident);
    }
    constexpr auto find_lua_module_key = "lua_newstate";
    bool is_lua_module(const char* module_path) {
        return Gum::Process::module_find_symbol_by_name(module_path, find_lua_module_key);
    }

    void attach_lua_vm(lua::state L) {
        debuggerAttach(L);
    }

    void wait_lua_module() {
        //TODO: support linux/macos
#ifdef _WIN32
        typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA {
            ULONG Flags;                    //Reserved.
            PCUNICODE_STRING FullDllName;   //The full path name of the DLL module.
            PCUNICODE_STRING BaseDllName;   //The base file name of the DLL module.
            PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
            ULONG SizeOfImage;              //The size of the DLL image, in bytes.
        } LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;
        typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
            ULONG Flags;                    //Reserved.
            PCUNICODE_STRING FullDllName;   //The full path name of the DLL module.
            PCUNICODE_STRING BaseDllName;   //The base file name of the DLL module.
            PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
            ULONG SizeOfImage;              //The size of the DLL image, in bytes.
        } LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;
        typedef union _LDR_DLL_NOTIFICATION_DATA {
            LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
            LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
        } LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;
        enum LdrDllNotificationReason : ULONG{
            LDR_DLL_NOTIFICATION_REASON_LOADED = 1,
            LDR_DLL_NOTIFICATION_REASON_UNLOADED
        };
        typedef VOID (CALLBACK *LdrDllNotification)(
        _In_     LdrDllNotificationReason                       NotificationReason,
        _In_     PLDR_DLL_NOTIFICATION_DATA const NotificationData,
        _In_opt_ PVOID                       Context
        );
        typedef NTSTATUS (NTAPI *LdrRegisterDllNotification)(
        _In_     ULONG                          Flags,
        _In_     LdrDllNotification NotificationFunction,
        _In_opt_ PVOID                          Context,
        _Out_    PVOID                          *Cookie
        );
        typedef NTSTATUS (NTAPI *LdrUnregisterDllNotification)(
        _In_ PVOID Cookie
        );
        struct DllNotification{
            LdrRegisterDllNotification dllRegisterNotification;
            LdrUnregisterDllNotification dllUnregisterNotification;
            PVOID Cookie;
            explicit operator bool() const {
                return dllRegisterNotification && dllUnregisterNotification;
            }
        };
        static DllNotification dllNotification = []()->DllNotification {
            HMODULE hmodule;
            if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "ntdll.dll", &hmodule)) {
                return {};
            }
            return {(LdrRegisterDllNotification)GetProcAddress(hmodule, "LdrRegisterDllNotification"), (LdrUnregisterDllNotification)GetProcAddress(hmodule, "LdrUnregisterDllNotification")} ;
        }();
        if (!dllNotification) {
            return;
        }
        dllNotification.dllRegisterNotification(0, [](LdrDllNotificationReason NotificationReason, PLDR_DLL_NOTIFICATION_DATA const NotificationData, PVOID Context){
            if (NotificationReason == LdrDllNotificationReason::LDR_DLL_NOTIFICATION_REASON_LOADED) {
                auto path = std::filesystem::path(std::wstring(NotificationData->Loaded.FullDllName->Buffer, NotificationData->Loaded.FullDllName->Length)).string();
                if (is_lua_module(path.c_str())){
                    RuntimeModule rm = {};
                    memcpy_s(rm.path, sizeof(rm.path), path.c_str(), path.size());
                    // find lua module lazy 
                    std::thread([](){
                        initialize(debuggerAttach, attachProcess);
                        if (dllNotification.Cookie)
                            dllNotification.dllUnregisterNotification(dllNotification.Cookie);
                    }).detach();
                }
            }
        }, NULL, &dllNotification.Cookie);
#endif
    }

    static RuntimeModule to_runtim_module(const Gum::ModuleDetails& details){
        RuntimeModule rm = {};
        const char* path = details.path();
        const char* name = details.name();
        auto range = details.range();
        strcpy(rm.path, path);
        strcpy(rm.name, name);
        rm.load_address = range.base_address;
        rm.size = range.size;
        return rm;
    }
    

    void initialize(fn_attach attach, bool ap) {
        log::init(ap);
        Gum::runtime_init();
        //auto gum_runtime = std::shared_ptr<void>(nullptr, [](auto){
        //    Gum::runtime_deinit();
        //});
        debuggerAttach = attach;
        attachProcess = ap;

        RuntimeModule rm = {};
#ifdef _WIN32
        auto addrs = Gum::SymbolUtil::find_matching_functions(find_lua_module_key, true);
        if (addrs.empty()) {
            wait_lua_module();
            log::info("can't find lua module");
            return;
        }
        if (addrs.size() > 1){
            log::info("find more than one lua module, random load the frist");
        }

        Gum::Process::enumerate_modules([&rm, addr = addrs[0]](const Gum::ModuleDetails& details)->bool{
            auto range = details.range();
            if (range.base_address < addr && addr < (void*)((intptr_t)range.base_address + range.size)) {
                rm = to_runtim_module(details);
                return false;
            }
            return true;
        });
#else
        Gum::Process::enumerate_modules([&rm](const Gum::ModuleDetails& details)->bool{
            if (is_lua_module(details.path())) {
                rm = to_runtim_module(details);
                return false;
            }
            return true;
        });
        if (!rm.load_address) {
            log::fatal("can't find lua module");
            return;
        }
#endif

        log::info(std::format("find lua module path:{}", rm.path).c_str());
        
        lua::lua_resolver r(rm.name);
        auto error_msg = lua::initialize(r);
        if (error_msg) {
            log::fatal(std::format("lua::initialize failed, can't find {}", error_msg).c_str());
            return;
        }

        auto luaversion = get_lua_version(rm.path);

        log::info(std::format("current lua version: {}", lua_version_to_string(luaversion)).c_str());

        auto vmhook = create_vmhook(luaversion);
        if (!vmhook->get_symbols(r)) {
           log::fatal("get_symbols failed");
           return;
        }
        //TODO: fix other thread pc
        vmhook->hook();
    }
}
