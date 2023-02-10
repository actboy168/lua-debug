#pragma once
#include <memory>
namespace autoattach::thread {
	struct threads_snapshot {
		virtual ~threads_snapshot() = default;
		virtual void suspend() = 0;
		virtual void resume() = 0;
	};
	std::unique_ptr<threads_snapshot> create_threads_snapshot();
}