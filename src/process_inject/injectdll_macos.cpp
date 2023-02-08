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
        "\xff\x03\x01\xd1\xf6\x57\x01\xa9\xf4\x4f\x02\xa9\xfd\x7b\x03\xa9\xfd\xc3\x00\x91\xf3\x03\x02\xaa\xf4\x03\x01\xaa\xf5\x03\x00\xaa\x60\x00\x3f\xd6\xa0\x12\x00\xb9\x22\x01\x00\x10\x1f\x20\x03\xd5\xe0\x23\x00\x91\x01\x00\x80\xd2\xe3\x03\x15\xaa\x80\x02\x3f\xd6\x00\x00\x80\x52\x60\x02\x3f\xd6\xfe\xff\xff\x17\xff\xc3\x05\xd1\xf8\x5f\x13\xa9\xf6\x57\x14\xa9\xf4\x4f\x15\xa9\xfd\x7b\x16\xa9\xfd\x83\x05\x91\x1f\x20\x03\xd5\xa8\x0e\x00\x58\x08\x01\x40\xf9\xa8\x83\x1c\xf8\xc0\x06\x00\xb4\xf3\x03\x00\xaa\xe0\x23\x00\x91\xe1\x03\x13\xaa\x02\x24\x80\x52\x42\x00\x00\x94\xf4\x1b\x40\xb9\xf7\x13\x40\xf9\x34\x02\x00\x34\x21\x08\x00\x10\x1f\x20\x03\xd5\x20\x00\x80\x92\xe0\x02\x3f\xd6\xf5\x03\x00\xaa\xe1\x07\x00\x70\x1f\x20\x03\xd5\x20\x00\x80\x92\xe0\x02\x3f\xd6\xf5\x00\x00\xb4\xf6\x03\x00\xaa\xa0\x00\x00\xb4\xe0\x03\x14\xaa\xa0\x02\x3f\xd6\xe0\x03\x14\xaa\xc0\x02\x3f\xd6\x21\x07\x00\x10\x1f\x20\x03\xd5\x20\x00\x80\x92\xe0\x02\x3f\xd6\xe8\x03\x00\xaa\xe0\x0b\x40\xf9\xc1\x00\x80\x52\x00\x01\x3f\xd6\xc0\x00\x00\xb4\xe8\x23\x00\x91\x01\x81\x00\x91\xe0\x02\x3f\xd6\x40\x00\x00\xb4\x00\x00\x3f\xd6\xe8\x13\x40\xf9\x61\x05\x00\x70\x1f\x20\x03\xd5\x20\x00\x80\x92\x00\x01\x3f\xd6\xf4\x03\x00\xaa\xe8\x13\x40\xf9\x21\x05\x00\x30\x1f\x20\x03\xd5\x20\x00\x80\x92\x00\x01\x3f\xd6\x00\x00\x40\xb9\xe2\x07\x40\xf9\xe1\x03\x13\xaa\x80\x02\x3f\xd6\xa8\x83\x5c\xf8\x1f\x20\x03\xd5\x49\x07\x00\x58\x29\x01\x40\xf9\x3f\x01\x08\xeb\x01\x01\x00\x54\x00\x00\x80\xd2\xfd\x7b\x56\xa9\xf4\x4f\x55\xa9\xf6\x57\x54\xa9\xf8\x5f\x53\xa9\xff\xc3\x05\x91\xc0\x03\x5f\xd6\x01\x00\x00\x94\x74\x68\x72\x65\x61\x64\x5f\x73\x75\x73\x70\x65\x6e\x64\x00\x74\x68\x72\x65\x61\x64\x5f\x74\x65\x72\x6d\x69\x6e\x61\x74\x65\x00\x64\x6c\x6f\x70\x65\x6e\x00\x76\x6d\x5f\x64\x65\x61\x6c\x6c\x6f\x63\x61\x74\x65\x00\x6d\x61\x63\x68\x5f\x74\x61\x73\x6b\x5f\x73\x65\x6c\x66\x5f\x00"
#elif defined(__x86_64__)
        "\x55\x48\x89\xe5\x41\x57\x41\x56\x53\x50\x48\x89\xd3\x49\x89\xf6\x49\x89\xff\xff\xd1\x41\x89\x47\x10\x48\x8d\x15\x20\x00\x00\x00\x48\x8d\x7d\xe0\x31\xf6\x4c\x89\xf9\x41\xff\xd6\x0f\x1f\x40\x00\x31\xff\xff\xd3\xeb\xfa\x66\x2e\x0f\x1f\x84\x00\x00\x00\x00\x00\x55\x48\x89\xe5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x81\xec\x28\x01\x00\x00\x48\x8b\x05\xbd\x01\x00\x00\x48\x8b\x00\x48\x89\x45\xd0\x48\x85\xff\x0f\x84\xd5\x00\x00\x00\x49\x89\xfe\x48\x8d\xbd\xb0\xfe\xff\xff\xba\x20\x01\x00\x00\x4c\x89\xf6\xe8\xee\x00\x00\x00\x44\x8b\xad\xc0\xfe\xff\xff\x48\x8b\x9d\xc8\xfe\xff\xff\x45\x85\xed\x74\x3c\x48\x8d\x35\xda\x00\x00\x00\x48\xc7\xc7\xfe\xff\xff\xff\xff\xd3\x49\x89\xc7\x48\x8d\x35\xd6\x00\x00\x00\x48\xc7\xc7\xfe\xff\xff\xff\xff\xd3\x4d\x85\xff\x74\x14\x49\x89\xc4\x48\x85\xc0\x74\x0c\x44\x89\xef\x41\xff\xd7\x44\x89\xef\x41\xff\xd4\x48\x8d\x35\xbe\x00\x00\x00\x48\xc7\xc7\xfe\xff\xff\xff\xff\xd3\x48\x8b\xbd\xb8\xfe\xff\xff\xbe\x06\x00\x00\x00\xff\xd0\x48\x85\xc0\x74\x13\x48\x8d\xb5\xd0\xfe\xff\xff\x48\x89\xc7\xff\xd3\x48\x85\xc0\x74\x02\xff\xd0\x48\x8d\x35\x8f\x00\x00\x00\x48\xc7\xc7\xfe\xff\xff\xff\xff\x95\xc8\xfe\xff\xff\x48\x89\xc3\x48\x8d\x35\x86\x00\x00\x00\x48\xc7\xc7\xfe\xff\xff\xff\xff\x95\xc8\xfe\xff\xff\x8b\x38\x48\x8b\x95\xb0\xfe\xff\xff\x4c\x89\xf6\xff\xd3\x48\x8b\x05\xd1\x00\x00\x00\x48\x8b\x00\x48\x3b\x45\xd0\x75\x14\x31\xc0\x48\x81\xc4\x28\x01\x00\x00\x5b\x41\x5c\x41\x5d\x41\x5e\x41\x5f\x5d\xc3\xe8\x01\x00\x00\x00\x74\x68\x72\x65\x61\x64\x5f\x73\x75\x73\x70\x65\x6e\x64\x00\x74\x68\x72\x65\x61\x64\x5f\x74\x65\x72\x6d\x69\x6e\x61\x74\x65\x00\x64\x6c\x6f\x70\x65\x6e\x00\x76\x6d\x5f\x64\x65\x61\x6c\x6c\x6f\x63\x61\x74\x65\x00\x6d\x61\x63\x68\x5f\x74\x61\x73\x6b\x5f\x73\x65\x6c\x66\x5f\x00"
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