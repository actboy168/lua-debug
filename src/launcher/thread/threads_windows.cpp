#include "threads.hpp"

namespace autoattach::thread {
    struct windows_threads_snapshot : threads_snapshot {
		~windows_threads_snapshot() override = default;

		void suspend() override{}
		void resume() override{}
	};
	std::unique_ptr<threads_snapshot> create_threads_snapshot(){
		return std::make_unique<windows_threads_snapshot>();
	}
}