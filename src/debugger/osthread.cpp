#include <debugger/osthread.h>
#include <debugger/impl.h>

namespace vscode {
	
	osthread::osthread(debugger_impl* dbg)
		: dbg_(dbg)
		, mtx_()
		, exit_(false)
		, thd_()
	{ }

	osthread::~osthread()
	{
		stop();
	}

	void osthread::start()
	{
		thd_.reset(new std::thread(std::bind(&osthread::run, this)));
	}

	void osthread::stop()
	{
		if (!exit_) {
			exit_ = true;
			if (thd_) thd_->join();
		}
	}

	void osthread::run()
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

	void osthread::lock() { return mtx_.lock(); }
	bool osthread::try_lock() { return mtx_.try_lock(); }
	void osthread::unlock() { return mtx_.unlock(); }
}
