#include <mach/mach_types.h>
#include <dlfcn.h>
#include <string>
#include <map>
#include <mach-o/dyld_images.h>
#include <mach-o/dyld.h>
#include <tuple>
#include <mach/mach_vm.h>

#define VIS_HIDDEN __attribute__((visibility("hidden")))

//FIXME: Refactor this out into a seperate file
struct VIS_HIDDEN RemoteBuffer {
    RemoteBuffer();

    RemoteBuffer(task_t task, mach_vm_address_t remote_address, size_t remote_size, bool allow_truncation);

    RemoteBuffer &operator=(RemoteBuffer &&other);

    ~RemoteBuffer();

    void *getLocalAddress() const;

    kern_return_t getKernelReturn() const;

    size_t getSize() const;

private:
    static std::pair<mach_vm_address_t, kern_return_t>
    map(task_t task, mach_vm_address_t remote_address, vm_size_t _size);

    static std::tuple<mach_vm_address_t, vm_size_t, kern_return_t> create(task_t task,
                                                                          mach_vm_address_t remote_address,
                                                                          size_t remote_size,
                                                                          bool allow_truncation);

    RemoteBuffer(std::tuple<mach_vm_address_t, vm_size_t, kern_return_t> T);

    mach_vm_address_t _localAddress;
    vm_size_t _size;
    kern_return_t _kr;
};

RemoteBuffer &RemoteBuffer::operator=(RemoteBuffer &&other) {
    std::swap(_localAddress, other._localAddress);
    std::swap(_size, other._size);
    std::swap(_kr, other._kr);
    return *this;
}

RemoteBuffer::RemoteBuffer() : _localAddress(0), _size(0), _kr(KERN_SUCCESS) {}

RemoteBuffer::RemoteBuffer(std::tuple<mach_vm_address_t, vm_size_t, kern_return_t> T)
        : _localAddress(std::get<0>(T)), _size(std::get<1>(T)), _kr(std::get<2>(T)) {}

RemoteBuffer::RemoteBuffer(task_t task, mach_vm_address_t remote_address, size_t remote_size, bool allow_truncation)
        : RemoteBuffer(RemoteBuffer::create(task, remote_address, remote_size, allow_truncation)) {};

std::pair<mach_vm_address_t, kern_return_t>
RemoteBuffer::map(task_t task, mach_vm_address_t remote_address, vm_size_t size) {
    static kern_return_t
    (*mvrn)(vm_map_t, mach_vm_address_t *, mach_vm_size_t, mach_vm_offset_t, int, vm_map_read_t, mach_vm_address_t,
            boolean_t, vm_prot_t *, vm_prot_t *, vm_inherit_t) = nullptr;
    vm_prot_t cur_protection = VM_PROT_NONE;
    vm_prot_t max_protection = VM_PROT_READ;
    if (size == 0) {
        return std::make_pair(MACH_VM_MIN_ADDRESS, KERN_INVALID_ARGUMENT);
    }
    mach_vm_address_t localAddress = 0;
#if TARGET_OS_SIMULATOR
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        mvrn = (kern_return_t (*)(vm_map_t, mach_vm_address_t*, mach_vm_size_t, mach_vm_offset_t, int, vm_map_read_t, mach_vm_address_t,
                                  boolean_t, vm_prot_t*, vm_prot_t*, vm_inherit_t))dlsym(RTLD_DEFAULT, "mach_vm_remap_new");
        if (mvrn == nullptr) {
            // We are running on a system that does not support task_read ports, use the old call
            mvrn = (kern_return_t (*)(vm_map_t, mach_vm_address_t*, mach_vm_size_t, mach_vm_offset_t, int, vm_map_read_t, mach_vm_address_t,
                                      boolean_t, vm_prot_t*, vm_prot_t*, vm_inherit_t))dlsym(RTLD_DEFAULT, "mach_vm_remap");
        }
    });
#else
    mvrn = &mach_vm_remap_new;
#endif
    auto kr = mvrn(mach_task_self(),
                   &localAddress,
                   size,
                   0,  // mask
                   VM_FLAGS_ANYWHERE | VM_FLAGS_RESILIENT_CODESIGN | VM_FLAGS_RESILIENT_MEDIA,
                   task,
                   remote_address,
                   true,
                   &cur_protection,
                   &max_protection,
                   VM_INHERIT_NONE);
    // The call is not succesfull return
    if (kr != KERN_SUCCESS) {
        return std::make_pair(MACH_VM_MIN_ADDRESS, kr);
    }
    // If it is not a shared buffer then copy it into a local buffer so our results are coherent in the event
    // the page goes way due to storage removal, etc. We have to do this because even after we read the page the
    // contents might go away of the object is paged out and then the backing region is disconnected (for example, if
    // we are copying some memory in the middle of a mach-o that is on a USB drive that is disconnected after we perform
    // the mapping). Once we copy them into a local buffer the memory will be handled by the default pager instead of
    // potentially being backed by the mmap pager, and thus will be guaranteed not to mutate out from under us.
    void *buffer = malloc(size + 1);
    if (buffer == nullptr) {
        (void) vm_deallocate(mach_task_self(), (vm_address_t) localAddress, size);
        return std::make_pair(MACH_VM_MIN_ADDRESS, KERN_NO_SPACE);
    }
    memcpy(buffer, (void *) localAddress, size);
    ((char *) buffer)[size] = 0; // Add a null terminator so strlcpy does not read base the end of the buffer.
    (void) vm_deallocate(mach_task_self(), (vm_address_t) localAddress, size);
    return std::make_pair((vm_address_t) buffer, KERN_SUCCESS);
}

std::tuple<mach_vm_address_t, vm_size_t, kern_return_t> RemoteBuffer::create(task_t task,
                                                                             mach_vm_address_t remote_address,
                                                                             size_t size,
                                                                             bool allow_truncation) {
    mach_vm_address_t localAddress;
    kern_return_t kr;
    // Try the initial map
    std::tie(localAddress, kr) = map(task, remote_address, size);
    if (kr == KERN_SUCCESS) return std::make_tuple(localAddress, size, kr);
    // The first attempt failed, truncate if possible and try again. We only need to try once since the largest
    // truncatable buffer we map is less than a single page. To be more general we would need to try repeatedly in a
    // loop.
    if (allow_truncation) {
        // Manually set to 4096 instead of page size to deal with wierd issues involving 4k page arm64 binaries
        size = 4096 - remote_address % 4096;
        std::tie(localAddress, kr) = map(task, remote_address, size);
        if (kr == KERN_SUCCESS) return std::make_tuple(localAddress, size, kr);
    }
    // If we reach this then the mapping completely failed
    return std::make_tuple(MACH_VM_MIN_ADDRESS, 0, kr);
}

RemoteBuffer::~RemoteBuffer() {
    if (!_localAddress) { return; }
    free((void *) _localAddress);
}

void *RemoteBuffer::getLocalAddress() const { return (void *) _localAddress; }

size_t RemoteBuffer::getSize() const { return _size; }

kern_return_t RemoteBuffer::getKernelReturn() const { return _kr; }

void withRemoteBuffer(task_t task, mach_vm_address_t remote_address, size_t remote_size, bool allow_truncation,
                      kern_return_t *kr, void (^block)(void *buffer, size_t size)) {
    kern_return_t krSink = KERN_SUCCESS;
    if (kr == nullptr) {
        kr = &krSink;
    }
    RemoteBuffer buffer(task, remote_address, remote_size, allow_truncation);
    *kr = buffer.getKernelReturn();
    if (*kr == KERN_SUCCESS) {
        block(buffer.getLocalAddress(), buffer.getSize());
    }
}

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
int32_t image_cputype;

bool get_all_images(task_t task) {
    allimages.clear();
    image_cputype = 0;
    dyld_process_functions functions;
    if (!functions)
        return false;

    kern_return_t ret;
    dyld_process_info info = functions.m_dyld_process_info_create(task, 0, &ret);

    if (info) {
        functions.m_dyld_process_info_for_each_image(info,
                                                     ^(uint64_t mach_header_addr, const uuid_t uuid, const char *path) {
                                                         if (image_cputype == 0) {
                                                             kern_return_t kr;
                                                             withRemoteBuffer(task, mach_header_addr,
                                                                              sizeof(mach_header), false, &kr,
                                                                              ^(void *buffer, size_t size) {
                                                                                  auto header = (struct mach_header *) buffer;
                                                                                  image_cputype = header->cputype;
                                                                              });
                                                         }
                                                         allimages.emplace(path, mach_header_addr);
                                                     });
        functions.m_dyld_process_info_release(info);
    }
    return true;
}

uint64_t get_module_address(const char *find_path) {
    auto iter = allimages.find(find_path);
    return iter != allimages.end() ? iter->second : 0;
}

bool is_same_arch(){
	auto target_arch =
#ifdef __x86_64__
    CPU_TYPE_X86_64;
#elif defined(__aarch64__)
    CPU_TYPE_ARM64;
#endif
	return target_arch == image_cputype;
}