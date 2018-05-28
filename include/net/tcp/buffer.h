#pragma once

#include <cassert>
#include <memory>
#include <net/queue.h>

namespace net { namespace tcp {

	template <typename T>
	class empty_atomic
	{
	public:
		inline empty_atomic() : ptr(nullptr) { }
		inline empty_atomic(T ptr_) : ptr(ptr_) { }
		inline ~empty_atomic() { }
		inline T exchange(T val) { T tmp = ptr; ptr = val; return tmp; }
	private:
		T ptr;
	private:
		empty_atomic(const empty_atomic&);
		empty_atomic& operator=(const empty_atomic&);
	};

	template <typename T, ::std::size_t N = 256, typename Alloc = ::std::allocator<T>>
	class buffer
		: public queue<T, N, Alloc, empty_atomic<queue_chunk<T, N>*>>
	{
		typedef queue<T, N, Alloc, empty_atomic<queue_chunk<T, N>*>> mybase;
		
	public:
		buffer()
			: mybase()
		{ }

		bool empty() const
		{
			return ((mybase::begin_chunk == mybase::back_chunk) && (mybase::begin_pos == mybase::back_pos));
		}

		size_t front_size() const
		{
			if (mybase::begin_chunk == mybase::back_chunk) {
				assert(mybase::begin_pos <= mybase::back_pos);
				return mybase::back_pos - mybase::begin_pos;
			}
			assert(mybase::begin_pos < N);
			return N - mybase::begin_pos;
		}

		size_t back_size() const
		{
			assert(mybase::back_pos < N);
			return N - mybase::back_pos;
		}

		void do_push(size_t n)
		{
			assert(n > 0);
			if (mybase::end_pos + n > N) {
				mybase::end_pos += n - 2;
				mybase::do_push();
				mybase::do_push();
				return;
			}
			assert(n <= N - mybase::end_pos);
			mybase::end_pos += n - 1;
			mybase::do_push();
		}

		void do_pop(size_t n)
		{
			assert(n > 0 && n <= N - mybase::begin_pos);
			mybase::begin_pos += n - 1;
			mybase::do_pop();
		}

	private:
		buffer(const buffer&);
		buffer& operator=(const buffer&);
	};
}}
