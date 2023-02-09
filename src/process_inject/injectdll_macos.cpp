#include <mach/error.h>
#include <mach/vm_types.h>
#include <cstddef> // for ptrdiff_t

#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach/mach.h>
#include <mach/processor.h>
#include <mach/processor_info.h>
#include <mach-o/fat.h> // for fat structure decoding
#include <fcntl.h> // for open/close
// for mmap()
#include <dlfcn.h>
#include <pthread_spis.h>
#include <stdio.h>
#include <string_view>

#include "injectdll.h"

#define LOG(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#define LOG_MACH(msg, err) mach_error(msg,err)

const char inject_code[] = {
#ifdef __aarch64__
       #include "macos/shellcode_arm64.h"
#elif defined(__x86_64__)
       #include "macos/shellcode_x64.h"
#else
#error "Unsupported architecture"
#endif
};
struct rmain_arg {           // dealloc
    size_t sizeofstruct;
    const char *name;       // dealloc
    thread_t injectThread; // kill this tread
    vm_address_t dlsym;
	char entrypoint[256];
};
static_assert(sizeof(vm_address_t) == sizeof(void*));

extern bool get_all_images(task_t task);

extern uint64_t get_module_address(const char *find_path);

vm_address_t get_symbol_address(void *ptr) {
    Dl_info info = {};
    if (dladdr(ptr, &info)) {
        auto base_address = get_module_address(info.dli_fname);
        auto offset = (intptr_t) ptr - (intptr_t) info.dli_fbase;
        return (intptr_t) base_address + offset;
    }
    return 0;
}
extern bool is_same_arch();
bool
mach_inject(
        pid_t targetProcess, const char *dylibPath, size_t len, std::string_view entry) {
    mach_port_t remoteTask = 0;
    mach_error_t err = task_for_pid(mach_task_self(), targetProcess, &remoteTask);
    if (err) {
        LOG_MACH("task_for_pid", err);
        return false;
    }

    if (!get_all_images(remoteTask)) {
        mach_port_deallocate(mach_task_self(), remoteTask);
        LOG("%s", "remote can't read all images");
        return false;
    }

    if (!is_same_arch()) {
        mach_port_deallocate(mach_task_self(), remoteTask);
        LOG("%s", "diff arch processor");
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

    vm_address_t remoteStack = (vm_address_t) NULL;
    vm_address_t remoteCode = (vm_address_t) NULL;
    vm_address_t remoteLibPath = (vm_address_t) NULL;
    vm_address_t remoteArg = (vm_address_t) NULL;

    auto align_fn = [](size_t c) {
        return ((c + 16) / 16) * 16;
    };

    size_t libPathSize = len * sizeof(char) + 1;
    constexpr
    size_t codeSize = sizeof(inject_code);
    constexpr
    size_t argSize = sizeof(rmain_arg);
    size_t want_size = stackSize + align_fn(libPathSize) + align_fn(argSize);

    vm_address_t remoteMem = (vm_address_t) NULL;
    //	Allocate the remoteStack.
    err = vm_allocate(remoteTask, &remoteMem, want_size, VM_FLAGS_ANYWHERE);
    if (err) {
        LOG_MACH("vm_allocate", err);
        goto freeport;
    }
    err = vm_protect(remoteTask, remoteMem, want_size, TRUE, VM_PROT_WRITE | VM_PROT_READ);
    if (err) {
        LOG_MACH("vm_protect", err);
        goto deallocateMem;
    }

    remoteArg = remoteMem;
    remoteLibPath = remoteArg + align_fn(argSize);
    remoteStack = remoteLibPath + align_fn(libPathSize);
    remoteStack = remoteStack - (remoteStack % 16);

    err = vm_allocate(remoteTask, &remoteCode, codeSize, VM_FLAGS_ANYWHERE);
    if (err) {
        LOG_MACH("vm_allocate", err);
        goto deallocateMem;
    }
    err = vm_write(remoteTask, remoteCode, (pointer_t) inject_code, codeSize);
    if (err) {
        LOG_MACH("vm_write", err);
        goto deallocateCode;
    }
    err = vm_protect(remoteTask, remoteCode, codeSize, FALSE, VM_PROT_EXECUTE | VM_PROT_READ);
    if (err) {
        LOG_MACH("vm_protect", err);
        goto deallocateCode;
    }


    err = vm_write(remoteTask, remoteLibPath, (pointer_t) dylibPath, libPathSize);
    if (err) {
        LOG_MACH("vm_write", err);
        goto deallocateCode;
    }

    arg.sizeofstruct = want_size;
    arg.name = (const char *) remoteLibPath;
    arg.dlsym = get_symbol_address((void *) &dlsym);

    err = vm_write(remoteTask, remoteArg, (pointer_t) & arg, sizeof(rmain_arg));
    if (err) {
        LOG_MACH("vm_write", err);
        goto deallocateCode;
    }


    //	Allocate the thread.
    remoteStack += (stackSize / 2); // this is the real stack
    // (*) increase the stack, since we're simulating a CALL instruction, which normally pushes return address on the stack
    remoteStack -= 16;

#ifdef __aarch64__

    remoteThreadState.__lr = 0; // invalid return address.

    remoteThreadState.__x[0] = (unsigned long long) (remoteArg);
    remoteThreadState.__x[1] = (unsigned long long) get_symbol_address((void*) &pthread_create_from_mach_thread);
    remoteThreadState.__x[2] = (unsigned long long) get_symbol_address((void*) &swtch_pri);
    remoteThreadState.__x[3] = (unsigned long long) get_symbol_address((void*) &mach_thread_self);

    // set remote Program Counter
    remoteThreadState.__pc = (unsigned long long) (remoteCode);

    // set remote Stack Pointer
    static_assert(sizeof(unsigned long long) == sizeof(remoteStack));
    remoteThreadState.__sp = (unsigned long long) remoteStack;

    // create thread and launch it
    err = thread_create_running(remoteTask, ARM_THREAD_STATE64,
                                (thread_state_t) &remoteThreadState, ARM_THREAD_STATE64_COUNT,
                                &remoteThread);
#elif defined(__x86_64__)

    remoteThreadState.__rdi = (unsigned long long) (remoteArg);
    remoteThreadState.__rsi = (unsigned long long) get_symbol_address((void*) &pthread_create_from_mach_thread);
    remoteThreadState.__rdx = (unsigned long long) get_symbol_address((void*) &swtch_pri);
    remoteThreadState.__rcx = (unsigned long long) get_symbol_address((void*) &mach_thread_self);
        // set remote Program Counter
    remoteThreadState.__rip = (unsigned long long) (remoteCode);

    // set remote Stack Pointer
    static_assert(sizeof(unsigned long long) == sizeof(remoteStack));
    remoteThreadState.__rsp = (unsigned long long) remoteStack;
    remoteThreadState.__rbp = (unsigned long long) remoteStack;

        // create thread and launch it
    err = thread_create_running(remoteTask, x86_THREAD_STATE64,
                                (thread_state_t) &remoteThreadState, x86_THREAD_STATE64_COUNT,
                                &remoteThread);
#endif
    if (!err) return true;
    LOG_MACH("thread_create_running", err);
deallocateCode:
    vm_deallocate(remoteTask, remoteCode, codeSize);
deallocateMem:
    vm_deallocate(remoteTask, remoteMem, want_size);
freeport:
    mach_port_deallocate(mach_task_self(), remoteTask);
    return false;
}

bool injectdll(pid_t pid, const std::string &dll, const char *entry) {
	std::string_view v(entry?entry:"");
	if (v.size() >= sizeof(rmain_arg::entrypoint)){
		LOG("entrypoint name[%s] is too long", entry);
		return false;
	}
#if defined(__aarch64__) || defined(__x86_64__)
    return mach_inject(pid, dll.c_str(), dll.length(), v);
#else
    return false;
#endif
}