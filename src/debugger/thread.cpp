#include <debugger/thread.h>
#include <debugger/impl.h>

namespace vscode {
	
	async::async(debugger_impl* dbg)
		: dbg_(dbg)
		, mtx_()
		, exit_(false)
		, thd_()
	{ }

	async::~async()
	{
		stop();
	}

	threadmode async::mode() const { return threadmode::async; }

	void async::start()
	{
		thd_.reset(new std::thread(std::bind(&async::run, this)));
	}

	void async::stop()
	{
		if (!exit_) {
			exit_ = true;
			if (thd_) thd_->join();
		}
	}

	void async::run()
	{
		for (
			; !exit_
			; std::this_thread::sleep_for(std::chrono::milliseconds(100)))
		{
			dbg_->update();
		}

		dbg_->close();
		dbg_->io_close();
	}

	void async::lock() { return mtx_.lock(); }
	bool async::try_lock() { return mtx_.try_lock(); }
	void async::unlock() { return mtx_.unlock(); }
	void async::update() { }

	sync::sync(debugger_impl* dbg)
		: dbg_(dbg)
		, time_()
		, last_(time_.now_ms())
	{ }

	threadmode sync::mode() const { return threadmode::sync; }
	void sync::start() {}
	void sync::stop() {
		dbg_->close();
		dbg_->io_close();
	}
	void sync::lock() {}
	bool sync::try_lock() { return true; }
	void sync::unlock() {}
	void sync::update() {
		uint64_t now = time_.now_ms();
		if (now - last_ > 100) {
			now = last_;
			dbg_->run_idle();
		}
	}
}
