#include "threads.hpp"

namespace autoattach::thread {
    struct linux_threads_snapshot : threads_snapshot {
		~linux_threads_snapshot() override = default;

		void suspend() override{}
		void resume() override{}
	};
	std::unique_ptr<threads_snapshot> create_threads_snapshot(){
		return std::make_unique<linux_threads_snapshot>();
	}
}