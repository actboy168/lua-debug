#include <mach/mach_types.h>
#include <dlfcn.h>
#include <string>
#include <map>
#include <mach-o/dyld_images.h>
#include <mach-o/dyld.h>

struct dyld_process_functions {
    void *(*m_dyld_process_info_create)(task_t task, uint64_t timestamp,
                                        kern_return_t *kernelError);

    void (*m_dyld_process_info_for_each_image)(
            void *info, void (^callback)(uint64_t machHeaderAddress,
                                         const uuid_t uuid, const char *path));

    void (*m_dyld_process_info_release)(void *info);

    dyld_process_functions() {
        m_dyld_process_info_create =
                (void *(*)(task_t task, uint64_t timestamp, kern_return_t *kernelError))
                        dlsym(RTLD_DEFAULT, "_dyld_process_info_create");
        m_dyld_process_info_for_each_image =
                (void (*)(void *info, void (^)(uint64_t machHeaderAddress,
                                               const uuid_t uuid, const char *path)))
                        dlsym(RTLD_DEFAULT, "_dyld_process_info_for_each_image");
        m_dyld_process_info_release =
                (void (*)(void *info)) dlsym(RTLD_DEFAULT, "_dyld_process_info_release");
    }

    explicit operator bool() const {
        return m_dyld_process_info_release && m_dyld_process_info_create && m_dyld_process_info_for_each_image;
    }
};

typedef void *dyld_process_info;
static std::map<std::string, uint64_t> allimages;

bool get_all_images(task_t task) {
    allimages.clear();
    dyld_process_functions functions;
    if (!functions)
        return false;
    
    kern_return_t ret;
    dyld_process_info info = functions.m_dyld_process_info_create(task, 0, &ret);
    if (info) {
        functions.m_dyld_process_info_for_each_image(info,
                                                     ^(uint64_t mach_header_addr, const uuid_t uuid, const char *path) {
                                                         allimages.emplace(path, mach_header_addr);
                                                     });
        functions.m_dyld_process_info_release(info);
    }
    return true;
}

uint64_t get_module_address(const char *find_path){
    auto iter = allimages.find(find_path);
    return iter != allimages.end() ? iter->second : 0;
}

