#include <debugger/thread.h>
#include <debugger/impl.h>

namespace vscode {
	
	dbg_thread::dbg_thread(debugger_impl* dbg)
		: dbg_(dbg)
		, mtx_()
		, exit_(false)
		, thd_()
	{ }

	dbg_thread::~dbg_thread()
	{
		stop();
	}

	void dbg_thread::start()
	{
		thd_.reset(new std::thread(std::bind(&dbg_thread::run, this)));
	}

	void dbg_thread::stop()
	{
		if (!exit_) {
			exit_ = true;
			if (thd_) thd_->join();
		}
	}

	void dbg_thread::run()
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

	void dbg_thread::lock() { return mtx_.lock(); }
	bool dbg_thread::try_lock() { return mtx_.try_lock(); }
	void dbg_thread::unlock() { return mtx_.unlock(); }
}
