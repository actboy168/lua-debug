#include <vector>
#include <mach/mach.h>
#include <mach/thread_info.h>

#include "threads.hpp"

namespace autoattach::thread {
    struct macos_threads_snapshot : threads_snapshot {
        thread_array_t thread_list = NULL;
		mach_msg_type_number_t thread_list_count = 0;
		std::vector<thread_t> vaildthreads;
		macos_threads_snapshot(){
			kern_return_t err = task_threads(mach_task_self(), &thread_list, &thread_list_count);
            if (err != KERN_SUCCESS) {
                mach_error("update_thread_list", err);
                return;
            }
			vaildthreads.reserve(thread_list_count);
		}
        ~macos_threads_snapshot() override {
			resume();
			 vm_size_t thread_list_size =
                    (vm_size_t)(thread_list_count * sizeof(thread_t));
            ::vm_deallocate(::mach_task_self(), (vm_address_t) thread_list,
                            thread_list_size);
		}

        void suspend() override {
			if (!thread_list_count || !thread_list)
				return;
			auto thread_self = mach_thread_self();
            for (mach_msg_type_number_t i = 0; i < thread_list_count; i++) {
				if (thread_self == thread_list[i]) continue;

				kern_return_t err = thread_suspend(thread_list[i]);
				if (err == KERN_SUCCESS) {
					vaildthreads.push_back(thread_list[i]);
				}
            }
			mach_port_deallocate(mach_task_self(), thread_self);
        }

        void resume() override {
            for (auto &&t : vaildthreads)
			{
				thread_resume(t);
			}
			vaildthreads.clear();
        }
    };

	std::unique_ptr<threads_snapshot> create_threads_snapshot() {
        return std::make_unique<macos_threads_snapshot>();
    }
}