#pragma once

#include <cassert>
#include <memory>

namespace net { namespace tcp {

	template <typename T, ::std::size_t N>
	struct buffer_chunk
	{
		T values [N];
		buffer_chunk<T, N>* next;
	};

	template <typename T, ::std::size_t N, typename Alloc>
	class buffer_alloc
		: public Alloc::template rebind<buffer_chunk<T, N> >::other
	{
	public:
		typedef buffer_chunk<T, N>                                  chunt_type;
		typedef typename Alloc::template rebind<chunt_type>::other base_type;

	public:
		inline chunt_type*  allocate()
		{
			return base_type::allocate(1);
		}

		inline void deallocate(chunt_type* p)
		{
			base_type::deallocate(p, 1);
		}
	};

	template <typename T, ::std::size_t N = 256, typename Alloc = ::std::allocator<T>>
	class buffer
		: public buffer_alloc<T, N, Alloc>
	{
		friend class sndbuffer;
		friend class rcvbuffer;
		
	public:
		typedef buffer_alloc<T, N, Alloc>       alloc_type;
		typedef typename alloc_type::chunt_type chunt_type;
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
			, spare_chunk(nullptr)
		{
			assert(begin_chunk);
			assert(empty());
		}

		~buffer()
		{
			clear();
			alloc_type::deallocate(begin_chunk);
			if (spare_chunk)
				alloc_type::deallocate(spare_chunk);
		}

		void clear()
		{
			for (;;) {
				if (begin_chunk == back_chunk) {
					break;
				}
				chunt_type *o = begin_chunk;
				begin_chunk = begin_chunk->next;
				alloc_type::deallocate(o);
			}
			begin_pos = 0;
			back_pos = 0;
			assert(begin_chunk);
			assert(empty());
		}

		reference front()
		{
			return begin_chunk->values[begin_pos];
		}

		const_reference front() const
		{
			return begin_chunk->values[begin_pos];
		}

		reference back()
		{
			return back_chunk->values[back_pos];
		}

		const_reference back() const
		{
			return back_chunk->values[back_pos];
		}

		void push(value_type&& val)
		{
			new(&back()) T(std::move(val));
			do_push();
		}

		void push(const_reference val)
		{
			new(&back()) T(val);
			do_push();
		}

		void pop()
		{
			assert(!empty());
			front().~T();
			do_pop();
		}

		bool try_pop(reference val)
		{
			if (empty())
				return false;
			val.~T();
			new(&val) T(front());
			pop();
			return true;
		}

		bool empty() const
		{
			return ((begin_chunk == back_chunk) && (begin_pos == back_pos));
		}

		size_t chunk_size() const
		{
			return N;
		}

	private:
		void do_push()
		{
			if (++back_pos != N)
				return;

			if (spare_chunk) {
				back_chunk->next = spare_chunk;
				spare_chunk = nullptr;
			} else {
				back_chunk->next = alloc_type::allocate();
				assert(back_chunk->next);
			}
			back_chunk = back_chunk->next;
			back_pos = 0;
		}

		void do_pop()
		{
			if (++ begin_pos == N) {
				chunt_type *o = begin_chunk;
				begin_chunk = begin_chunk->next;
				begin_pos = 0;
				if (spare_chunk)
					alloc_type::deallocate(spare_chunk);
				spare_chunk = o;
			}
		}

	private:
		chunt_type* begin_chunk;
		size_t      begin_pos;
		chunt_type* back_chunk;
		size_t      back_pos;
		chunt_type* spare_chunk;

	private:
		buffer(const buffer&);
		buffer& operator=(const buffer&);
	};
}}
