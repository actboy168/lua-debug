
#include <dlfcn.h>
#include <mach/mach.h>
#include <pthread_spis.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
struct rmain_arg {  // dealloc
    size_t sizeofstruct;
    const char* name;       // dealloc
    thread_t injectThread;  // kill this tread
    decltype(&dlsym) dlsym;
    char entrypoint[256];

    void* get_func(const char* func_name) {
        return this->dlsym(RTLD_DEFAULT, func_name);
    }
};

static void* rmain(void* ptr) {
    if (!ptr) {
        return 0;
    }
    rmain_arg& arg = *(rmain_arg*)ptr;
    // terminate inject thread
    if (arg.injectThread) {
        auto _thread_suspend   = (decltype(&thread_suspend))arg.get_func("thread_suspend");
        auto _thread_terminate = (decltype(&thread_terminate))arg.get_func("thread_terminate");
        if (_thread_suspend && _thread_terminate) {
            _thread_suspend(arg.injectThread);
            _thread_terminate(arg.injectThread);
        }
    }
    void* handler = nullptr;
    auto _dlerror = (decltype(&dlerror))arg.get_func("dlerror");
    // load lib
    {
        auto _dlopen = (decltype(&dlopen))arg.get_func("dlopen");
        handler      = _dlopen((const char*)arg.name, RTLD_NOW | RTLD_LOCAL);
    }
    if (!handler) {
        auto ec       = (const char*)_dlerror();
        auto _fprintf = (decltype(&fprintf))arg.get_func("fprintf");
        auto _stderr  = *(FILE**)arg.get_func("__stderrp");
        _fprintf(_stderr, "%s\n", ec);
        auto _exit = (decltype(&exit))arg.get_func("exit");
        _exit(1);
    }
    else {
        void (*func)();
        func = (decltype(func))arg.dlsym(handler, arg.entrypoint);
        if (func)
            func();
    }
    // delete memory
    {
        auto _vm_deallocate = (decltype(&vm_deallocate))arg.get_func("vm_deallocate");
        auto task_self_     = *(decltype(mach_task_self_)*)arg.get_func("mach_task_self_");
        _vm_deallocate(task_self_, (intptr_t)ptr, arg.sizeofstruct);
    }
    return 0;
}

void inject [[noreturn]] (rmain_arg* ptr, decltype(&pthread_create_from_mach_thread) pthread_create_from_mach_thread, decltype(&swtch_pri) swtch_pri, decltype(&mach_thread_self) mach_thread_self) {
    ptr->injectThread = mach_thread_self();
    pthread_t thread;
    pthread_create_from_mach_thread(&thread, NULL, rmain, ptr);

    while (1) {
        swtch_pri(0);
    }
}
}
