#include <fcntl.h>  // for open/close
#include <mach-o/dyld.h>
#include <mach-o/fat.h>  // for fat structure decoding
#include <mach-o/getsect.h>
#include <mach/error.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/processor.h>
#include <mach/processor_info.h>
#include <mach/vm_types.h>
#include <sys/sysctl.h>

#include <cstddef>  // for ptrdiff_t
#include <cassert>
// for mmap()
#include <dlfcn.h>
#include <pthread_spis.h>
#include <stdio.h>

#include <string_view>

#include "injectdll.h"
#include "shellcode.inl"

#define LOG_FMT(fmt, ...) fprintf(stderr, fmt "\n", __VA_ARGS__)
#define LOG(msg) fprintf(stderr, "%s\n", msg)
#define LOG_MACH(msg, err) mach_error(msg, err)
#ifndef NDEBUG
#    define DEBUG_LOG_FMT(fmt, ...) LOG_FMT(fmt, __VA_ARGS__)
#else
#    define DEBUG_LOG_FMT(fmt, ...) ((void)0)
#endif

constexpr const char magic[] = "lua_debug.injecto_help.inject_code.magic0x9F332";
static_assert(sizeof(magic) % 16 == 0);
constexpr auto code_size = sizeof(magic) + sizeof(inject_code);

struct rmain_arg {  // dealloc
    size_t sizeofstruct;
    const char* name;       // dealloc
    thread_t injectThread;  // kill this tread
    vm_address_t dlsym;
    char entrypoint[256];
};
static_assert(sizeof(vm_address_t) == sizeof(void*));

extern bool get_all_images(task_t task);

extern uint64_t get_module_address(const char* find_path);

vm_address_t get_symbol_address(void* ptr) {
    Dl_info info = {};
    if (dladdr(ptr, &info)) {
        auto base_address = get_module_address(info.dli_fname);
        auto offset       = (intptr_t)ptr - (intptr_t)info.dli_fbase;
        return (intptr_t)base_address + offset;
    }
    return 0;
}
#ifndef CPU_TYPE_ARM64
#    define CPU_TYPE_ARM ((cpu_type_t)12)
#    define CPU_ARCH_ABI64 0x01000000
#    define CPU_TYPE_ARM64 (CPU_TYPE_ARM | CPU_ARCH_ABI64)
#endif
uint64_t injector__get_system_arch() {
    size_t size;
    cpu_type_t type      = -1;
    int mib[CTL_MAXNAME] = { 0 };
    size_t length        = CTL_MAXNAME;

    if (sysctlnametomib("sysctl.proc_cputype", mib, &length) != 0) {
        return CPU_TYPE_ANY;
    }

    mib[length] = getpid();
    length++;
    size = sizeof(cpu_type_t);

    if (sysctl(mib, (u_int)length, &type, &size, 0, 0) != 0) {
        return CPU_TYPE_ANY;
    }
    return type;
}
#ifndef P_TRANSLATED
#    define P_TRANSLATED 0x00020000
#endif
uint64_t injector__get_process_arch(pid_t pid) {
    int mib[CTL_MAXNAME]        = { 0 };
    mib[0]                      = CTL_KERN;
    mib[1]                      = KERN_PROC;
    mib[2]                      = KERN_PROC_PID;
    mib[3]                      = pid;
    size_t length               = 4;
    struct kinfo_proc proc_info = {};
    size_t size                 = sizeof(proc_info);

    if (sysctl(mib, (u_int)length, &proc_info, &size, NULL, 0) != 0) {
        return CPU_TYPE_ANY;
    }

    if (P_TRANSLATED == (P_TRANSLATED & proc_info.kp_proc.p_flag)) {
        if (P_LP64 == (P_LP64 & proc_info.kp_proc.p_flag)) {
            return CPU_TYPE_X86_64;
        }
        else {
            return CPU_TYPE_I386;
        }
    }
    else {
        return injector__get_system_arch();
    }
}

vm_address_t scan_inject_code(mach_port_t task) {
    kern_return_t kr;
    mach_vm_address_t address = MACH_VM_MIN_ADDRESS;
    mach_vm_size_t size       = 0;
    natural_t depth           = 0;

    while (TRUE) {
        struct vm_region_submap_info_64 info;
        mach_msg_type_number_t info_count;

        while (TRUE) {
            info_count = VM_REGION_SUBMAP_INFO_COUNT_64;
            kr         = mach_vm_region_recurse(task, &address, &size, &depth, (vm_region_recurse_info_t)&info, &info_count);
            if (kr != KERN_SUCCESS)
                break;

            if (info.is_submap) {
                depth++;
                continue;
            }
            else {
                break;
            }
        }

        if (kr != KERN_SUCCESS)
            break;
        constexpr auto target_prot = VM_PROT_EXECUTE | VM_PROT_READ;
        if ((info.protection & target_prot) == target_prot) {
            vm_offset_t ptr;
            mach_msg_type_number_t dataCnt;
            if (vm_read(task, address, code_size, &ptr, &dataCnt) == KERN_SUCCESS) {
                if (dataCnt >= code_size && strncmp((const char*)ptr, magic, sizeof(magic)) == 0) {
                    DEBUG_LOG_FMT("find inject code at %p\n", (void*)address);
                    auto code_address = (const char*)ptr + sizeof(magic);
                    auto ec           = strncmp(code_address, inject_code, sizeof(inject_code));
                    if (ec == 0) {
                        return (vm_address_t)address + sizeof(magic);
                    }
                    else {
                        // changed version, free old
                        DEBUG_LOG_FMT("free old inject code at %p\n", (void*)address);
                        assert(vm_deallocate(task, address, size) == KERN_SUCCESS);
                        return false;
                    }
                }
            }
        }

        address += size;
        size = 0;
    }
    return 0;
}

vm_address_t allocate_inject_code(task_t remoteTask) {
    mach_error_t err;
    vm_address_t remoteCode = 0;
    constexpr auto codeSize = sizeof(inject_code);
    constexpr auto needSize = code_size;
    if (err = vm_allocate(remoteTask, &remoteCode, needSize, VM_FLAGS_ANYWHERE); err) {
        LOG_MACH("vm_allocate", err);
        return 0;
    }
    if (err = vm_write(remoteTask, remoteCode, (pointer_t)magic, sizeof(magic)); err) {
        LOG_MACH("vm_write", err);
        goto deallocateCode;
    }
    remoteCode += sizeof(magic);
    if (err = vm_write(remoteTask, remoteCode, (pointer_t)inject_code, codeSize); err) {
        LOG_MACH("vm_write", err);
        goto deallocateCode;
    }
    if (err = vm_protect(remoteTask, remoteCode, codeSize, FALSE, VM_PROT_EXECUTE | VM_PROT_READ); err) {
        LOG_MACH("vm_protect", err);
        goto deallocateCode;
    }
    return remoteCode;
deallocateCode:
    assert(vm_deallocate(remoteTask, remoteCode, codeSize) == KERN_SUCCESS);
    return 0;
}

bool mach_inject(
    pid_t targetProcess,
    const char* dylibPath,
    size_t len,
    std::string_view entry
) {
    mach_port_t remoteTask = 0;
    mach_error_t err       = task_for_pid(mach_task_self(), targetProcess, &remoteTask);
    if (err) {
        LOG_MACH("task_for_pid", err);
        return false;
    }

    if (!get_all_images(remoteTask)) {
        mach_port_deallocate(mach_task_self(), remoteTask);
        LOG("remote can't read all images");
        return false;
    }
    auto targetArch = injector__get_process_arch(targetProcess);
    auto systemArch = injector__get_system_arch();
    if (targetArch != systemArch) {
        mach_port_deallocate(mach_task_self(), remoteTask);
        if (targetArch == CPU_TYPE_X86_64 && systemArch == CPU_TYPE_ARM64) {
            LOG("can't inject x86_64 on rosetta");
            return false;
        }
        LOG("diff arch processor");
        return false;
    }

    size_t stackSize = 16 * 1024;
    rmain_arg arg;
    bzero(&arg, sizeof(rmain_arg));
    thread_act_t remoteThread;
    memcpy(arg.entrypoint, entry.data(), entry.size());
    arg.entrypoint[entry.size()] = '\0';

#ifdef __aarch64__
    arm_thread_state64_t remoteThreadState;
#elif defined(__x86_64__)
    x86_thread_state64_t remoteThreadState;
#endif
    bzero(&remoteThreadState, sizeof(remoteThreadState));

    vm_address_t remoteStack   = (vm_address_t)NULL;
    vm_address_t remoteLibPath = (vm_address_t)NULL;
    vm_address_t remoteArg     = (vm_address_t)NULL;

    auto align_fn = [](size_t c) {
        return ((c + 16) / 16) * 16;
    };

    size_t libPathSize       = len * sizeof(char) + 1;
    constexpr size_t argSize = sizeof(rmain_arg);
    size_t want_size         = stackSize + align_fn(libPathSize) + align_fn(argSize);

    vm_address_t remoteMem = (vm_address_t)NULL;

    vm_address_t remoteCode = (vm_address_t)NULL;
    if (remoteCode = scan_inject_code(remoteTask); remoteCode == 0) {
        if (remoteCode = allocate_inject_code(remoteTask); remoteCode == 0) {
            goto freeport;
        }
    }
    //	Allocate the remoteStack.
    if (err = vm_allocate(remoteTask, &remoteMem, want_size, VM_FLAGS_ANYWHERE); err) {
        LOG_MACH("vm_allocate", err);
        goto freeport;
    }
    if (err = vm_protect(remoteTask, remoteMem, want_size, TRUE, VM_PROT_WRITE | VM_PROT_READ); err) {
        LOG_MACH("vm_protect", err);
        goto deallocateMem;
    }

    remoteArg     = remoteMem;
    remoteLibPath = remoteArg + align_fn(argSize);
    remoteStack   = remoteLibPath + align_fn(libPathSize);
    remoteStack   = remoteStack - (remoteStack % 16);

    arg.sizeofstruct = want_size;
    arg.name         = (const char*)remoteLibPath;
    arg.dlsym        = get_symbol_address((void*)&dlsym);

    if (err = vm_write(remoteTask, remoteLibPath, (pointer_t)dylibPath, libPathSize); err) {
        LOG_MACH("vm_write", err);
        goto deallocateMem;
    }

    if (err = vm_write(remoteTask, remoteArg, (pointer_t)&arg, sizeof(rmain_arg)); err) {
        LOG_MACH("vm_write", err);
        goto deallocateMem;
    }

    //	Allocate the thread.
    remoteStack += (stackSize / 2);  // this is the real stack
    // (*) increase the stack, since we're simulating a CALL instruction, which normally pushes return address on the stack
    remoteStack -= 16;

#ifdef __aarch64__

    remoteThreadState.__lr = 0;  // invalid return address.

    remoteThreadState.__x[0] = (unsigned long long)(remoteArg);
    remoteThreadState.__x[1] = (unsigned long long)get_symbol_address((void*)&pthread_create_from_mach_thread);
    remoteThreadState.__x[2] = (unsigned long long)get_symbol_address((void*)&swtch_pri);
    remoteThreadState.__x[3] = (unsigned long long)get_symbol_address((void*)&mach_thread_self);

    // set remote Program Counter
    remoteThreadState.__pc = (unsigned long long)(remoteCode);

    // set remote Stack Pointer
    static_assert(sizeof(unsigned long long) == sizeof(remoteStack));
    remoteThreadState.__sp = (unsigned long long)remoteStack;

    // create thread and launch it
    err = thread_create_running(remoteTask, ARM_THREAD_STATE64, (thread_state_t)&remoteThreadState, ARM_THREAD_STATE64_COUNT, &remoteThread);
#elif defined(__x86_64__)

    remoteThreadState.__rdi = (unsigned long long)(remoteArg);
    remoteThreadState.__rsi = (unsigned long long)get_symbol_address((void*)&pthread_create_from_mach_thread);
    remoteThreadState.__rdx = (unsigned long long)get_symbol_address((void*)&swtch_pri);
    remoteThreadState.__rcx = (unsigned long long)get_symbol_address((void*)&mach_thread_self);
    // set remote Program Counter
    remoteThreadState.__rip = (unsigned long long)(remoteCode);

    // set remote Stack Pointer
    static_assert(sizeof(unsigned long long) == sizeof(remoteStack));
    remoteThreadState.__rsp = (unsigned long long)remoteStack;
    remoteThreadState.__rbp = (unsigned long long)remoteStack;

    // create thread and launch it
    err = thread_create_running(remoteTask, x86_THREAD_STATE64, (thread_state_t)&remoteThreadState, x86_THREAD_STATE64_COUNT, &remoteThread);
#endif
    if (err == KERN_SUCCESS) {
        assert(mach_port_deallocate(mach_task_self(), remoteThread) == KERN_SUCCESS);
        goto freeport;
    }
    LOG_MACH("thread_create_running", err);
deallocateMem:
    assert(vm_deallocate(remoteTask, remoteMem, want_size) == KERN_SUCCESS);
freeport:
    assert(mach_port_deallocate(mach_task_self(), remoteTask) == KERN_SUCCESS);
    return err == KERN_SUCCESS;
}

bool injectdll(pid_t pid, const std::string& dll, const char* entry) {
    std::string_view v(entry ? entry : "");
    if (v.size() >= sizeof(rmain_arg::entrypoint)) {
        LOG_FMT("entrypoint name[%s] is too long", entry);
        return false;
    }
#if defined(__aarch64__) || defined(__x86_64__)
    return mach_inject(pid, dll.c_str(), dll.length(), v);
#else
    return false;
#endif
}