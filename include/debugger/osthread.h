#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <memory>
#include <debugger/debugger.h>

namespace vscode {

	class debugger_impl;

	class semaphore {
	public:
		semaphore(int count = 0)
			: count_(count) {
		}

		void signal() {
			std::unique_lock<std::mutex> lock(mutex_);
			if (++count_ <= 0) {
				cv_.notify_one();
			}
		}

		void wait() {
			std::unique_lock<std::mutex> lock(mutex_);
			if (--count_ < 0) {
				cv_.wait(lock);
			}
		}

	private:
		std::mutex mutex_;
		std::condition_variable cv_;
		int count_;
	};

	struct osthread
	{
		osthread(debugger_impl* dbg);
		~osthread();
		void start();
		void stop();
		void run();
		void lock();
		bool try_lock();
		void unlock();
		bool is_current();
		void join();

		debugger_impl*               dbg_;
		std::mutex                   mtx_;
		std::atomic<bool>            exit_;
		std::unique_ptr<std::thread> thd_;
	};
}
