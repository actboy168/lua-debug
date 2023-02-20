#include "autoattach.h"
#include <lua.hpp>
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
#include <dobby.h>
#include <string>
#include <string_view>
#include <memory>
#include <charconv>
#include <filesystem>

#include "common.hpp"
#include "hook/hook_common.h"
#include "symbol_resolver/symbol_resolver.h"
#include "thread/threads.hpp"
class ProcessRuntimeUtility {
public:
    static const std::vector<RuntimeModule> &GetProcessModuleMap();
};
namespace autoattach {
	std::mutex lockLoadDll;
	fn_attach debuggerAttach;
	bool      attachProcess = false;
    static attach_args args;

#ifdef _WIN32
	FARPROC luaapi(const char* name) {
        return nullptr;
	}
#endif
    bool iequals(const std::string_view &a, const std::string_view &b) {
        return std::equal(a.begin(), a.end(),
                          b.begin(), b.end(),
                          [](char a, char b) {
                              return tolower(a) == tolower(b);
                          });
    }

    lua_version get_lua_version_from_env() {
        const char *env_version = getenv("LUA_DEBUG_VERSION");
        if (env_version) {
            auto env = std::string_view(env_version);
            if (iequals(env, "jit"))
                return lua_version::luajit;
            if (iequals(env, "5.1"))
                return lua_version::lua51;
            if (iequals(env, "5.2"))
                return lua_version::lua52;
            if (iequals(env, "5.3"))
                return lua_version::lua53;
            if (iequals(env, "5.4") || iequals(env, "latest"))
                return lua_version::lua54;
        }
        return lua_version::unknown;
    }
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
        if (DobbySymbolResolver(path, "luaJIT_version_2_1_0_beta3")) {
            return lua_version::luajit;
        }
        if (DobbySymbolResolver(path, "luaJIT_version_2_1_0_beta2")) {
            return lua_version::luajit;
        }
        if (DobbySymbolResolver(path, "luaJIT_version_2_1_0_beta1")) {
            return lua_version::luajit;
        }
        if (DobbySymbolResolver(path, "luaJIT_version_2_1_0_alpha")) {
            return lua_version::luajit;
        }
		auto p = DobbySymbolResolver(path, "lua_ident");;
        const char *lua_ident = (const char *) p;
        if (!lua_ident)
            return lua_version::unknown;
		return get_lua_version_from_ident(lua_ident);
    }

    bool is_lua_module(const RuntimeModule &module, bool signature) {
        if (signature) {
            //TODO:
        }
        return DobbySymbolResolver(module.path, "luaL_newstate");
    }

    void attach_lua_vm(lua_State *L) {
        debuggerAttach(L, &args);
    }

    bool is_signature_mode() {
        return getenv("LUA_DEBUG_SIGNATURE") != nullptr;
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
                RuntimeModule rm = {};
                auto path = std::filesystem::path(std::wstring(NotificationData->Loaded.FullDllName->Buffer, NotificationData->Loaded.FullDllName->Length)).string();
                memcpy_s(rm.path,sizeof(rm.path), path.c_str(), path.size());
                if (is_lua_module(rm, is_signature_mode())){
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
    

    void initialize(fn_attach attach, bool ap) {
#ifndef NDEBUG
        log_set_level(0);
#endif
        debuggerAttach = attach;
        attachProcess = ap;
		DobbyUpdateModuleMap();

        const auto& modulemap = ProcessRuntimeUtility::GetProcessModuleMap();
        bool signature = is_signature_mode();
        RuntimeModule rm = {};

        for (const auto &module: modulemap) {
            if (is_lua_module(module, signature)) {
                //check lua version
                rm = module;
                break;
            }
        }
        if (!rm.load_address) {
            wait_lua_module();
            LOG("can't find lua module");
            return;
        }
		auto symbol_resolver = symbol_resolver::symbol_resolver_factory_create(rm);
        if (!args.get_symbols(symbol_resolver.get())) {
            LOG("can't load args symbols");
            return;
        }

        auto luaversion = get_lua_version_from_env();
        if (luaversion == lua_version::unknown) {
            luaversion = get_lua_version(rm.path);
        }

        auto vmhook = create_vmhook(luaversion);
        if (!vmhook->get_symbols(symbol_resolver)) {
            LOG("get_symbols failed");
            return;
        }
        //TODO: fix other thread pc
#if defined(TARGET_ARCH_ARM) || defined(TARGET_ARCH_ARM64)
        dobby_enable_near_branch_trampoline();
#endif
        vmhook->hook();
#if defined(TARGET_ARCH_ARM) || defined(TARGET_ARCH_ARM64)
        dobby_disable_near_branch_trampoline();
#endif
    }
}
