#include <autoattach/wait_dll.h>

#ifdef _WIN32

#    include <bee/nonstd/filesystem.h>
#    include <windows.h>
#    include <winternl.h>

namespace luadebug::autoattach {
    bool wait_dll(bool (*loaded)(std::string const&)) {
        typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA {
            ULONG Flags;                   // Reserved.
            PCUNICODE_STRING FullDllName;  // The full path name of the DLL module.
            PCUNICODE_STRING BaseDllName;  // The base file name of the DLL module.
            PVOID DllBase;                 // A pointer to the base address for the DLL in memory.
            ULONG SizeOfImage;             // The size of the DLL image, in bytes.
        } LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;
        typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
            ULONG Flags;                   // Reserved.
            PCUNICODE_STRING FullDllName;  // The full path name of the DLL module.
            PCUNICODE_STRING BaseDllName;  // The base file name of the DLL module.
            PVOID DllBase;                 // A pointer to the base address for the DLL in memory.
            ULONG SizeOfImage;             // The size of the DLL image, in bytes.
        } LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;
        typedef union _LDR_DLL_NOTIFICATION_DATA {
            LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
            LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
        } LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;
        enum LdrDllNotificationReason : ULONG {
            LDR_DLL_NOTIFICATION_REASON_LOADED = 1,
            LDR_DLL_NOTIFICATION_REASON_UNLOADED
        };
        typedef VOID(CALLBACK * LdrDllNotification)(
            _In_ LdrDllNotificationReason NotificationReason,
            _In_ PLDR_DLL_NOTIFICATION_DATA const NotificationData,
            _In_opt_ PVOID Context
        );
        typedef NTSTATUS(NTAPI * LdrRegisterDllNotification)(
            _In_ ULONG Flags,
            _In_ LdrDllNotification NotificationFunction,
            _In_opt_ PVOID Context,
            _Out_ PVOID * Cookie
        );
        typedef NTSTATUS(NTAPI * LdrUnregisterDllNotification)(
            _In_ PVOID Cookie
        );
        struct DllNotification {
            LdrRegisterDllNotification dllRegisterNotification;
            LdrUnregisterDllNotification dllUnregisterNotification;
            PVOID Cookie;
            explicit operator bool() const {
                return dllRegisterNotification && dllUnregisterNotification;
            }
        };
        static DllNotification dllNotification = []() -> DllNotification {
            HMODULE hmodule;
            if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "ntdll.dll", &hmodule)) {
                return {};
            }
            return { (LdrRegisterDllNotification)GetProcAddress(hmodule, "LdrRegisterDllNotification"), (LdrUnregisterDllNotification)GetProcAddress(hmodule, "LdrUnregisterDllNotification") };
        }();
        if (!dllNotification) {
            return false;
        }
        dllNotification.dllRegisterNotification(
            0, [](LdrDllNotificationReason NotificationReason, PLDR_DLL_NOTIFICATION_DATA const NotificationData, PVOID Context) {
                if (NotificationReason == LdrDllNotificationReason::LDR_DLL_NOTIFICATION_REASON_LOADED) {
                    auto path = fs::path(std::wstring(NotificationData->Loaded.FullDllName->Buffer, NotificationData->Loaded.FullDllName->Length)).string();
                    auto f    = (decltype(loaded))Context;
                    if (f(path)) {
                        if (dllNotification.Cookie)
                            dllNotification.dllUnregisterNotification(dllNotification.Cookie);
                    }
                }
            },
            loaded, &dllNotification.Cookie
        );
        return true;
    }
}

#else

// TODO: support linux/macos

namespace luadebug::autoattach {
    bool wait_dll(bool (*loaded)(std::string const&)) {
        return false;
    }
}

#endif
