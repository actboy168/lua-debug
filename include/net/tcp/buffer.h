#pragma once

#include <cassert>
#include <memory>

namespace net { namespace tcp {

	template <typename T, ::std::size_t N>
	struct buffer_chunk
	{
		T values[N];
		buffer_chunk<T, N>* next;
	};

	template <typename T, ::std::size_t N, typename Alloc>
	class buffer_alloc
		: public Alloc::template rebind<buffer_chunk<T, N> >::other
	{
	public:
		typedef buffer_chunk<T, N>                                  chunk_type;
		typedef typename Alloc::template rebind<chunk_type>::other base_type;

	public:
		chunk_type * allocate()
		{
			return base_type::allocate(1);
		}

		void deallocate(chunk_type* p)
		{
			base_type::deallocate(p, 1);
		}
	};

	template <typename T, ::std::size_t N = 256, typename Alloc = ::std::allocator<T>>
	class buffer
		: public buffer_alloc<T, N, Alloc>
	{
	public:
		typedef buffer_alloc<T, N, Alloc>       alloc_type;
		typedef typename alloc_type::chunk_type chunk_type;
		typedef T                               value_type;
		typedef value_type*                     pointer;
		typedef value_type&                     reference;
		typedef value_type const&               const_reference;

	public:
		buffer()
			: begin_chunk(alloc_type::allocate())
			, begin_pos(0)
			, back_chunk(begin_chunk)
			, back_pos(0)
			, end_chunk(begin_chunk)
			, end_pos(0)
			, spare_chunk()
		{
			assert(begin_chunk);
			do_push();
			assert(empty());
		}

		virtual ~buffer() {
			clear();
			alloc_type::deallocate(begin_chunk);
			if (spare_chunk) {
				alloc_type::deallocate(spare_chunk);
				spare_chunk = nullptr;
			}
		}

		void clear() {
			for (;;) {
				if (begin_chunk == end_chunk) {
					break;
				}
				chunk_type *o = begin_chunk;
				begin_chunk = begin_chunk->next;
				alloc_type::deallocate(o);
			}
		}

		reference front() {
			return begin_chunk->values[begin_pos];
		}

		const_reference front() const {
			return begin_chunk->values[begin_pos];
		}

		reference back() {
			return back_chunk->values[back_pos];
		}

		const_reference back() const {
			return back_chunk->values[back_pos];
		}

		void push(value_type&& val) {
			new(&back()) T(::std::move(val));
			do_push();
		}

		void push(const_reference val) {
			new(&back()) T(val);
			do_push();
		}

		void pop() {
			assert(!empty());
			front().~T();
			do_pop();
		}

		bool try_pop(reference val) {
			if (empty())
				return false;
			val.~T();
			new(&val) T(front());
			pop();
			return true;
		}

		void do_push() {
			back_chunk = end_chunk;
			back_pos = end_pos;

			if (++end_pos != N)
				return;

			if (spare_chunk) {
				end_chunk->next = spare_chunk;
				spare_chunk = nullptr;
			}
			else {
				end_chunk->next = alloc_type::allocate();
				assert (end_chunk->next);
			}
			end_chunk = end_chunk->next;
			end_pos = 0;
		}

		void do_pop() {
			if (++begin_pos == N) {
				chunk_type *o = begin_chunk;
				begin_chunk = begin_chunk->next;
				begin_pos = 0;
				if (spare_chunk) {
					alloc_type::deallocate(o);
				}
				else {
					spare_chunk = o;
				}
			}
		}

		bool empty() const {
			return (begin_chunk == back_chunk) && (begin_pos == back_pos);
		}

		size_t front_size() const {
			if (begin_chunk == back_chunk) {
				assert(begin_pos <= back_pos);
				return back_pos - begin_pos;
			}
			assert(begin_pos < N);
			return N - begin_pos;
		}

		size_t back_size() const {
			assert(back_pos < N);
			return N - back_pos;
		}

		void do_push(size_t n) {
			assert(n > 0);
			if (end_pos + n > N) {
				end_pos += n - 2;
				do_push();
				do_push();
				return;
			}
			assert(n <= N - end_pos);
			end_pos += n - 1;
			do_push();
		}

		void do_pop(size_t n) {
			assert(n > 0 && n <= N - begin_pos);
			begin_pos += n - 1;
			do_pop();
		}

	protected:
		chunk_type* begin_chunk;
		size_t      begin_pos;
		chunk_type* back_chunk;
		size_t      back_pos;
		chunk_type* end_chunk;
		size_t      end_pos;
		chunk_type* spare_chunk;

	private:
		buffer(const buffer&);
		buffer& operator=(const buffer&);
	};
}}
