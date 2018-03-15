#pragma once

#include <base/hook/assembler/register.h>
#include <base/hook/assembler/operand.h>
#include <array>
#include <cassert>
#include <Windows.h>

namespace base { namespace hook { namespace assembler {

	struct basic_writer
	{
		basic_writer(uint8_t* buf, size_t len)
			: beg_(buf)
			, cur_(buf)
			, max_(len)
		{ }

		template <class T>
		inline void emit(T val)
		{
			static_assert(std::is_integral<T>::value, "emit's param must be integer.");
			assert((_size() + sizeof T) <= max_);

			*reinterpret_cast<T*>(cur_) = val;
			cur_ += sizeof T;
		}

		inline void emit(const void* buf, size_t len)
		{
			assert((_size() + len) <= max_);
			memcpy(cur_, buf, len);
			cur_ += len;
		}

		void emit_operand(reg r, const operand& adr)
		{
			assert(adr.len_ > 0);

			emit<uint8_t>((adr.buf_[0] & ~0x38) | (r.code() << 3));

			for (size_t i = 1; i < adr.len_; i++)
			{
				emit<uint8_t>(adr.buf_[i]);
			}
		}

		void mov(reg dst, uint32_t imm32)
		{
			emit<uint8_t>(0xB8 | dst.code());
			emit(imm32);
		}

		void mov(reg dst, const operand& src)
		{
			emit<uint8_t>(0x8B);
			emit_operand(dst, src);
		}

		void mov(reg dst, reg src)
		{
			emit<uint8_t>(0x89);
			emit<uint8_t>(0xC0 | src.code() << 3 | dst.code());
		}

		void push(uint32_t imm32)
		{
			emit<uint8_t>(0x68);
			emit<uint32_t>(imm32);
		}

		void push(reg src)
		{
			emit<uint8_t>(0x50 | src.code());
		}

		void pop(reg dst)
		{
			emit<uint8_t>(0x58 | dst.code());
		}

		void add(const operand& dst, uint8_t imm8)
		{
			emit<uint8_t>(0x83);
			emit_operand(eax, dst);
			emit<uint8_t>(imm8);
		}

		void jmp(uintptr_t jmp_dst, uintptr_t jmp_src)
		{
			emit<uint8_t>(0xE9);
			emit<uint32_t>(jmp_dst - (jmp_src + 5));
		}

		void call(uintptr_t call_dst, uintptr_t call_src)
		{
			emit<uint8_t>((uint8_t)0xE8);
			emit<uint32_t>((call_dst)-(call_src + 5));
		}

		void ret(uint16_t imm16 = 0)
		{
			if (imm16 == 0)
			{
				emit<uint8_t>(0xC3);
			}
			else
			{
				emit<uint8_t>(0xC2);
				emit<uint8_t>(imm16 & 0xFF);
				emit<uint8_t>((imm16 >> 8) & 0xFF);
			}
		}

		void _clear()
		{
			cur_ = beg_;
		}

		const void* _data() const
		{
			return beg_;
		}

		size_t _size() const
		{
			return cur_ - beg_;
		}

		size_t _maxsize() const
		{
			return max_;
		}

		void _seek(size_t n) {
			assert(n >= _size());
			assert(n < max_);
			cur_ = beg_ + n;
		}

		bool _executable()
		{
			DWORD protect = 0;
			if (!::VirtualProtectEx(::GetCurrentProcess(), (void*)this, max_, PAGE_EXECUTE_READWRITE, &protect))
			{
				return false;
			}

			if (!::FlushInstructionCache(::GetCurrentProcess(), (void*)this, max_))
			{
				return false;
			}

			return true;
		}

		uint8_t* beg_;
		uint8_t* cur_;
		size_t   max_;
	};

	template <size_t BufSizeT = 256>
	struct writer
		: public basic_writer
	{
		std::array<uint8_t, BufSizeT> data;

		writer()
			: basic_writer(data.data(), data.size())
		{ }
	};

	struct dynwriter
		: public basic_writer
	{
		dynwriter(size_t n)
			: basic_writer(new uint8_t[n], n)
		{ }

		~dynwriter() { delete basic_writer::beg_; }
	};
}}}
