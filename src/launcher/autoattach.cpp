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

#include "common.hpp"
#include "hook/hook_common.h"
#include "symbol_resolver/symbol_resolver.h"
#include "lua_resolver.h"
#include <gumpp.hpp>
namespace autoattach {
	std::mutex lockLoadDll;
	fn_attach debuggerAttach;
	bool      attachProcess = false;

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
        /*
            luaJIT_version_2_1_0_beta3
            luaJIT_version_2_1_0_beta2
            luaJIT_version_2_1_0_beta1
            luaJIT_version_2_1_0_alpha
        */
        if (!Gum::SymbolUtil::find_matching_functions("luaJIT_version_2_1_0*").empty()){
            return lua_version::luajit;
        }
		auto p = Gum::Process::module_find_symbol_by_name(path, "lua_ident");;
        const char *lua_ident = (const char *) p;
        if (!lua_ident)
            return lua_version::unknown;
		return get_lua_version_from_ident(lua_ident);
    }

    bool is_lua_module(const char* module_path, bool signature) {
        if (signature) {
            //TODO:
        }
        return Gum::Process::module_find_symbol_by_name(module_path, "luaL_newstate");
    }

    void attach_lua_vm(lua::state L) {
        debuggerAttach(L);
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
                auto path = std::filesystem::path(std::wstring(NotificationData->Loaded.FullDllName->Buffer, NotificationData->Loaded.FullDllName->Length)).string();
                if (is_lua_module(path.c_str(), is_signature_mode())){
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
    

    void initialize(fn_attach attach, bool ap) {
        Gum::runtime_init();
        auto gum_runtime = std::shared_ptr<void>(nullptr, [](auto){
            Gum::runtime_deinit();
        });
        debuggerAttach = attach;
        attachProcess = ap;

        bool signature = is_signature_mode();
        RuntimeModule rm = {};
        Gum::Process::enumerate_modules([&rm, signature](const Gum::ModuleDetails& details)->bool{
            const char* path = details.path();
            if (is_lua_module(path, signature)){
                memcpy(rm.path, path, strlen(path));
                auto range = details.range();
                rm.load_address = range.base_address;
                rm.size = range.size;
                return false;
            }
            return true;
        });
        if (!rm.load_address) {
            wait_lua_module();
            LOG("can't find lua module");
            return;
        }
        LOG(std::format("find lua module path:{}", rm.path).c_str());
        
        lua::lua_resolver r(rm);
        lua::initialize(r);

        auto luaversion = get_lua_version_from_env();
        if (luaversion == lua_version::unknown) {
            luaversion = get_lua_version(rm.path);
        }
        LOG(std::format("current lua version: {}", lua_version_to_string(luaversion)).c_str());

        auto vmhook = create_vmhook(luaversion);
        if (!vmhook->get_symbols(r.context)) {
           LOG("get_symbols failed");
           return;
        }
        //TODO: fix other thread pc
        vmhook->hook();
    }
}
