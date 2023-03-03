#include <autoattach/autoattach.h>
#include <autoattach/luaversion.h>
#include <util/common.hpp>
#include <util/log.h>
#include <hook/hook_common.h>
#include <resolver/lua_resolver.h>

#include <mutex>
#include <stack>
#include <set>
#include <vector>
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
#include <filesystem>
#include <cstring>
#include <gumpp.hpp>

namespace luadebug::autoattach {
	std::mutex lockLoadDll;
	fn_attach debuggerAttach;
	bool      attachProcess = false;

    constexpr auto find_lua_module_key = "lua_newstate";
    bool is_lua_module(const char* module_path) {
        if (Gum::Process::module_find_export_by_name(module_path, find_lua_module_key))
            return true;
        return Gum::Process::module_find_symbol_by_name(module_path, find_lua_module_key) != nullptr;
    }

    int attach_lua_vm(lua::state L) {
        return debuggerAttach(L);
    }

    bool wait_lua_module() {
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
            return false;
        }
        dllNotification.dllRegisterNotification(0, [](LdrDllNotificationReason NotificationReason, PLDR_DLL_NOTIFICATION_DATA const NotificationData, PVOID Context){
            if (NotificationReason == LdrDllNotificationReason::LDR_DLL_NOTIFICATION_REASON_LOADED) {
                auto path = std::filesystem::path(std::wstring(NotificationData->Loaded.FullDllName->Buffer, NotificationData->Loaded.FullDllName->Length)).string();
                if (is_lua_module(path.c_str())){
                    // find lua module lazy 
                    std::thread([](){
                        initialize(debuggerAttach, attachProcess);
                        if (dllNotification.Cookie)
                            dllNotification.dllUnregisterNotification(dllNotification.Cookie);
                    }).detach();
                }
            }
        }, NULL, &dllNotification.Cookie);
        return true;
#endif
        return false;
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
        log::info("initialize");
        Gum::runtime_init();
        //auto gum_runtime = std::shared_ptr<void>(nullptr, [](auto){
        //    Gum::runtime_deinit();
        //});
        debuggerAttach = attach;
        attachProcess = ap;

        RuntimeModule rm = {};
        Gum::Process::enumerate_modules([&rm](const Gum::ModuleDetails& details)->bool{
            if (is_lua_module(details.path())) {
                rm = to_runtim_module(details);
                return false;
            }
            return true;
        });
        if (!rm.load_address) {
            if (!wait_lua_module())
                log::fatal("can't find lua module");
            return;
        }

        log::info(std::format("find lua module path:{}", rm.path).c_str());
        
        lua_resolver r(rm.path);
        auto error_msg = lua::initialize(r);
        if (error_msg) {
            log::fatal(std::format("lua::initialize failed, can't find {}", error_msg).c_str());
            return;
        }

        auto luaversion = get_lua_version(rm);

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
