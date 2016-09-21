#pragma once

#include <net/poller/event.h>
#include <net/datetime/clock.h>
#include <limits>
#include <map>

namespace net { namespace poller {

	class timer_queue
	{
	public:
		timer_queue()
			: clock()
			, now(0)
			, timers()
			, timers_ref()
		{ }

		uint64_t add_timer(int timeout, event_t* e, uint64_t id)
		{
			uint64_t expiration = ((now != 0) ? now: clock.now_ms()) + timeout;
			timer_info_t info = {e, id};
			timers.insert(std::make_pair(expiration, info));
			bool suc = timers_ref[(uint64_t)e].insert(std::make_pair(id, expiration)).second;
			assert(suc); (int)suc;
			return expiration;
		}

		void cancel_timer(event_t* e, uint64_t id, uint64_t expiration)
		{
			cancel_timer_(e, id, expiration);
			timers_ref[(uint64_t)e].erase(id);
		}

		void cancel_timer(event_t* e, uint64_t id)
		{
			cancel_timer_(e, id, timers_ref[(uint64_t)e][id]);
			timers_ref[(uint64_t)e].erase(id);
		}

		void cancel_timer(event_t* e)
		{
			auto eit = timers_ref.find((uint64_t)e);
			if (eit != timers_ref.end())
			{
				for (auto it = eit->second.begin(); it != eit->second.end(); ++it)
				{
					cancel_timer_(e, it->first, it->second);
				}
				timers_ref.erase(eit);
			}
		}

	protected:
		int64_t execute_timers()
		{
			if (timers.empty())
				return (std::numeric_limits<int64_t>::max)();

			uint64_t realnow = clock.now_ms();

			for (;;)
			{
				auto it = timers.begin();
				if (it == timers.end())
					return (std::numeric_limits<int64_t>::max)();
				if (it == timers.end() || it->first > realnow)
					return it->first - realnow;

				now = it->first;

				event_t* e = it->second.e;
				uint64_t id = it->second.id;
				timers.erase(it);
				timers_ref[(uint64_t)e].erase(id);
				e->event_timer(id);

				now = 0;
			}
		}

		void cancel_timer_(event_t* e, uint64_t id, uint64_t expiration)
		{
			for (auto it = timers.find(expiration); (it != timers.end()) && (it->first == expiration); ++it)
			{
				if (it->second.e == e && it->second.id == id)
				{
					timers.erase(it);
					return;
				}
			}
			assert(false);
		}

		void cancel_timer_(event_t* e, uint64_t id)
		{
			for (auto it = timers.begin(); it != timers.end(); ++it)
			{
				if (it->second.e == e && it->second.id == id)
				{
					timers.erase(it);
					return;
				}
			}
			assert(false);
		}

	private:
		datetime::clock_t clock;
		uint64_t now;

		struct timer_info_t
		{
			event_t* e;
			uint64_t id;
		};
		std::multimap<uint64_t, timer_info_t> timers;
		std::map<uint64_t, std::map<uint64_t, uint64_t>> timers_ref;

		timer_queue(const timer_queue&);
		const timer_queue &operator = (const timer_queue&);
	};
}}
