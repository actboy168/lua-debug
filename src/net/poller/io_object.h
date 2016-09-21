#pragma once

#include <map>

namespace net { namespace poller {

	template <class Poller>
	struct io_object_t
	{
		typedef Poller poller_type;

		io_object_t(poller_type* poller, event_t* event)
			: poller_(poller)
			, event_(event)
			, ud_()
		{ }

		virtual ~io_object_t()
		{
			cancel_timer();
		}

		const poller_type* get_poller() const
		{
			return poller_;
		}

		poller_type* get_poller()
		{
			return poller_;
		}

		void set_fd(socket::fd_t s)
		{
			poller_->add_fd(s, event_, &ud_);
		}

		void rm_fd()
		{
			poller_->rm_fd(event_, &ud_);
		}

		void set_pollin()
		{
			poller_->set_pollin(event_, &ud_);
		}

		void reset_pollin()
		{
			poller_->reset_pollin(event_, &ud_);
		}

		void set_pollout()
		{
			poller_->set_pollout(event_, &ud_);
		}

		void reset_pollout()
		{
			poller_->reset_pollout(event_, &ud_);
		}

		void add_timer(int timeout, uint64_t id)
		{
			poller_->add_timer(timeout, event_, id);
		}

		void cancel_timer(uint64_t id)
		{
			poller_->cancel_timer(event_, id);
		}

		void cancel_timer()
		{
			poller_->cancel_timer(event_);
		}

		poller_type* poller_;
		event_t*   event_;
		typename poller_type::ud_t ud_;
	};
}}
