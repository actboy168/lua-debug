#pragma once

#include <string_view>
#include <type_traits>
#include <utility>

namespace luadebug::lua {
    template <unsigned short N>
    struct cstring {
        constexpr explicit cstring(std::string_view str) noexcept
            : cstring { str, std::make_integer_sequence<unsigned short, N> {} } {}
        constexpr const char* data() const noexcept { return chars_; }
        constexpr unsigned short size() const noexcept { return N; }
        constexpr operator std::string_view() const noexcept { return { data(), size() }; }
        template <unsigned short... I>
        constexpr cstring(std::string_view str, std::integer_sequence<unsigned short, I...>) noexcept
            : chars_ { str[I]..., '\0' } {}
        char chars_[static_cast<size_t>(N) + 1];
    };
    template <>
    struct cstring<0> {
        constexpr explicit cstring(std::string_view) noexcept {}
        constexpr const char* data() const noexcept { return nullptr; }
        constexpr unsigned short size() const noexcept { return 0; }
        constexpr operator std::string_view() const noexcept { return {}; }
    };
    namespace function {
        constexpr auto name_f(std::string_view name) noexcept {
#if defined(_MSC_VER)
            size_t i = name.find('<');
            for (;;) {
                i = name.find("lua", i);
                for (size_t n = i; n < name.size(); ++n) {
                    if (name[n] == ' ') {
                        i = n;
                        break;
                    }
                    if (name[n] == '(') {
                        name.remove_prefix(i);
                        name.remove_suffix(name.size() - (n - i));
                        return name;
                    }
                }
            }
            return std::string_view {};
#else
            size_t i = name.find('[');
            i        = name.find("lua", i);
            name.remove_prefix(i);
            i = name.find(']');
            name.remove_suffix(name.size() - i);
            return name;
#endif
        }
        template <auto F>
        constexpr auto name_f() noexcept {
#if defined(_MSC_VER)
            constexpr auto name = name_f({ __FUNCSIG__, sizeof(__FUNCSIG__) });
#else
            constexpr auto name = name_f({ __PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) });
#endif
            return cstring<name.size()> { name };
        }
        template <auto F>
        inline constexpr auto name_v = name_f<F>();
    }
    template <auto F>
    constexpr std::string_view function_name() noexcept {
        using T = decltype(F);
        static_assert(std::is_pointer_v<T>);
        static_assert(std::is_function_v<std::remove_pointer_t<T>>);
        return function::name_v<F>;
    }
    template <auto F>
    inline constexpr auto function_name_v = function_name<F>();
}
