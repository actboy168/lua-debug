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
			return ((begin_chunk == back_chunk) && (begin_pos == back_pos));
		}

		size_t front_size() const
		{
			if (begin_chunk == back_chunk) {
				assert(begin_pos <= back_pos);
				return back_pos - begin_pos;
			}
			assert(begin_pos < N);
			return N - begin_pos;
		}

		size_t back_size() const
		{
			assert(back_pos < N);
			return N - back_pos;
		}

		void do_push(size_t n)
		{
			assert(n > 0 && n <= N - back_pos);
			back_pos += n - 1;
			mybase::do_push();
		}

		void do_pop(size_t n)
		{
			assert(n > 0 && n <= N - begin_pos);
			begin_pos += n - 1;
			mybase::do_pop();
		}

	private:
		buffer(const buffer&);
		buffer& operator=(const buffer&);
	};
}}
