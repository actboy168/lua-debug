#include <autoattach/wait_dll.h>
#ifdef _WIN32

#    include <bee/nonstd/filesystem.h>
#    include <windows.h>
#    include <winternl.h>
#else
#    include <gumpp.hpp>
#    ifdef __APPLE__
#        include <mach-o/dyld.h>
#        include <mach-o/dyld_images.h>
#        include <mach/mach.h>

#        include <set>
struct _GumDarwinAllImageInfos {
    int format;

    uint64_t info_array_address;
    size_t info_array_count;
    size_t info_array_size;

    uint64_t notification_address;

    bool libsystem_initialized;

    uint64_t dyld_image_load_address;

    uint64_t shared_cache_base_address;
};
extern "C" bool gum_darwin_query_all_image_infos(mach_port_t task, _GumDarwinAllImageInfos* infos);
#    endif
#endif
namespace luadebug::autoattach {
#ifdef _WIN32
    bool wait_dll(bool (*loaded)(const std::string &)) {
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
#elif defined(__APPLE__)
    using WaitDllCallBack_t = bool (*)(const std::string&);
    struct WaitDllListener : Gum::NoLeaveInvocationListener {
        WaitDllCallBack_t loaded;
        Gum::RefPtr<Gum::Interceptor> interceptor;
        std::set<std::string> loaded_dlls;
        WaitDllListener() {
            auto count = _dyld_image_count();
            for (size_t i = 0; i < count; i++) {
                loaded_dlls.emplace(_dyld_get_image_name(i));
            }
        }
        ~WaitDllListener() override = default;
        void dyld_image_notifier(enum dyld_image_mode mode, uint32_t infoCount, const struct dyld_image_info info[]) {
            if (mode != dyld_image_mode::dyld_image_adding) {
                return;
            }
            for (size_t i = 0; i < infoCount; i++) {
                auto [iter, created] = loaded_dlls.emplace(info[i].imageFilePath);
                if (created) {
                    if (loaded(info[i].imageFilePath)) {
                        interceptor->detach(this);
                        delete this;
                        return;
                    }
                }
            }
        }
        void on_enter(Gum::InvocationContext* context) override {
            auto mode      = (dyld_image_mode)(intptr_t)context->get_nth_argument_ptr(0);
            auto infoCount = (uint32_t)(intptr_t)context->get_nth_argument_ptr(1);
            auto info      = (const struct dyld_image_info*)context->get_nth_argument_ptr(2);
            dyld_image_notifier(mode, infoCount, info);
        }
    };
    bool wait_dll(WaitDllCallBack_t loaded) {
        _GumDarwinAllImageInfos infos = {};
        if (!gum_darwin_query_all_image_infos(mach_task_self(), &infos))
            return false;
        auto interceptor = Gum::Interceptor_obtain();
        if (!interceptor)
            return false;
        auto listener         = new WaitDllListener;
        listener->loaded      = loaded;
        listener->interceptor = interceptor;
        return interceptor->attach((void*)infos.notification_address, listener, nullptr);
    }
#else

    // TODO: support linux

    bool wait_dll(bool (*loaded)(const std::string &)) {
        return false;
    }
#endif
}
