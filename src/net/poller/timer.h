#pragma once

#include <map>
#include <net/poller/event.h>

namespace net { namespace poller {

	template <class Poller>
	struct timer_t
		: public event_t
	{
		typedef Poller poller_type;

		timer_t(poller_type* poller)
			: poller_(poller)
		{ }

		virtual ~timer_t()
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

		bool event_in() { return true; }
		bool event_out() { return true; }
		void event_close() { }
		void event_timer(uint64_t id) = 0;

		void add_timer(int timeout, uint64_t id)
		{
			poller_->add_timer(timeout, this, id);
		}

		void cancel_timer(uint64_t id)
		{
			poller_->cancel_timer(this, id);
		}

		void cancel_timer()
		{
			poller_->cancel_timer(this);
		}

		poller_type* poller_;
	};
}}
