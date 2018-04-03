#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <net/datetime/clock.h>
#include <debugger/debugger.h>

namespace vscode {

	class debugger_impl;

	struct dbg_thread
	{
		virtual threadmode mode() const = 0;
		virtual void start() = 0;
		virtual void stop() = 0;
		virtual void update() = 0;
		virtual void lock() = 0;
		virtual bool try_lock() = 0;
		virtual void unlock() = 0;
	};

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

	struct async
		: public dbg_thread
	{
		async(debugger_impl* dbg);
		~async();
		threadmode mode() const;
		void start();
		void stop();
		void run();
		void lock();
		bool try_lock();
		void unlock();
		void update();

		debugger_impl*               dbg_;
		std::mutex                   mtx_;
		std::atomic<bool>            exit_;
		std::unique_ptr<std::thread> thd_;
	};

	struct sync
		: public dbg_thread
	{
		sync(debugger_impl* dbg);
		threadmode mode() const;
		void start();
		void stop();
		void lock();
		bool try_lock();
		void unlock();
		void update();

		debugger_impl*         dbg_;
		net::datetime::clock_t time_;
		uint64_t               last_;
	};
}
