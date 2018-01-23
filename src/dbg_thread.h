#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include <net/datetime/clock.h>

namespace vscode {

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
		async(std::function<void()> const& threadfunc)
			: mtx_()
			, exit_(false)
			, func_(threadfunc)
			, thd_()
		{ }

		~async()
		{
			exit_ = true;
			if (thd_) thd_->join();
		}

		threadmode mode() const { return threadmode::async; }

		void start()
		{
			thd_.reset(new std::thread(std::bind(&async::run, this)));
		}

		void run()
		{
			for (
				; !exit_
				; std::this_thread::sleep_for(std::chrono::milliseconds(100)))
			{
				func_();
			}
		}

		void lock() { return mtx_.lock(); }
		bool try_lock() { return mtx_.try_lock(); }
		void unlock() { return mtx_.unlock(); }
		void update() { }

		std::mutex  mtx_;
		std::atomic<bool> exit_;
		std::function<void()> func_;
		std::unique_ptr<std::thread> thd_;
	};

	struct sync
		: public dbg_thread
	{
		sync(std::function<void()> const& threadfunc)
			: func_(threadfunc)
			, time_()
			, last_(time_.now_ms())
		{ }

		threadmode mode() const { return threadmode::sync; }
		void start() {}
		void lock() {}
		bool try_lock() { return true; }
		void unlock() {}
		void update() {
			uint64_t now = time_.now_ms();
			if (now - last_ > 100) {
				now = last_;
				func_();
			}
		}
		std::function<void()>  func_;
		net::datetime::clock_t time_;
		uint64_t               last_;
	};
}
