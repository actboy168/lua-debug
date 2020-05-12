#pragma once

#include <type_traits>

namespace base {
	namespace call_ {
		template <class OutputClass, class InputClass>
		struct same_size {
			static const bool value = 
				(!std::is_reference<InputClass>::value && sizeof(OutputClass) == sizeof(InputClass))
				|| (std::is_reference<InputClass>::value && sizeof(OutputClass) == sizeof(std::add_pointer<InputClass>::type));
		};

		template <class OutputClass, class InputClass>
		union cast_union {
			OutputClass out;
			InputClass in;
		};

		template <class Argument>
		inline uintptr_t cast(const Argument input, typename std::enable_if<std::is_function<typename std::remove_reference<Argument>::type>::value, void>::type* = 0) {
			cast_union<uintptr_t, Argument> u;
			static_assert(std::is_trivially_copyable<Argument>::value, "Argument is not trivially copyable.");
			static_assert((sizeof(Argument) == sizeof(u)) && (sizeof(Argument) == sizeof(uintptr_t)), "Argument and uintptr_t are not the same size.");
			u.in = input;
			return u.out;
		}

		template <class Argument>
		inline uintptr_t cast(const Argument input, typename std::enable_if<!std::is_function<typename std::remove_reference<Argument>::type>::value && same_size<uintptr_t, Argument>::value, void>::type* = 0) {
			cast_union<uintptr_t, Argument> u;
			static_assert(std::is_trivially_copyable<Argument>::value, "Argument is not trivially copyable.");
			u.in = input;
			return u.out;
		}

		template <class Argument>
		inline uintptr_t cast(const Argument input, typename std::enable_if<!std::is_function<typename std::remove_reference<Argument>::type>::value && !same_size<uintptr_t, Argument>::value, void>::type* = 0) {
			static_assert(std::is_trivially_copyable<Argument>::value, "Argument is not trivially copyable.");
			static_assert(sizeof(Argument) < sizeof(uintptr_t), "Argument can not be converted to uintptr_t.");
			return static_cast<uintptr_t>(input);
		}

		template <typename Arg>
		struct cast_type {
			typedef uintptr_t type;
		};
	}

	template <typename R, typename F, typename ... Args>
	inline R std_call(F f, Args ... args) {
		return (reinterpret_cast<R(__stdcall *)(typename call_::cast_type<Args>::type ... args)>(f))(call_::cast(args)...);
	}

	template <typename R, typename F, typename ... Args>
	inline R fast_call(F f, Args ... args) {
		return (reinterpret_cast<R(__fastcall *)(typename call_::cast_type<Args>::type ... args)>(f))(call_::cast(args)...);
	}

	template <typename R, typename F, typename ... Args>
	inline R c_call(F f, Args ... args) {
		return (reinterpret_cast<R(__cdecl *)(typename call_::cast_type<Args>::type ... args)>(f))(call_::cast(args)...);
	}

	template <typename R, typename F, typename This, typename ... Args>
	inline R this_call(F f, This t, Args ... args) {
		return (reinterpret_cast<R(__fastcall *)(typename call_::cast_type<This>::type, void*, typename call_::cast_type<Args>::type ... args)>(f))(call_::cast(t), 0, call_::cast(args)...);
	}
}
