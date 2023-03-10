#pragma once

#include <lua.hpp>
#include <vector>
#include <functional>
#include <type_traits>
#include <string_view>
#include <stdint.h>

namespace luadebug::lua {
    using state = uintptr_t;
    using cfunction = int (*) (state);
    using kcontext = LUA_KCONTEXT;
    using kfunction = int (*) (state, int, kcontext);
    using debug = uintptr_t;
    using hook = void (*) (state, debug);

    struct resolver {
        virtual intptr_t find(std::string_view name) const = 0;
    };
}

namespace luadebug::lua::impl {
    template <class T>
    struct conv { using type = T; };
    template <class T>
    using conv_t = typename conv<T>::type;
    template <class Sig>
    struct invocable;
    template <class R, class...Args>
    struct invocable<R(Args...)> {
        using type = conv_t<R>(*)(conv_t<Args>...);
    };
    template <class R, class...Args>
    struct invocable<R(*)(Args...)> {
        using type = conv_t<R>(*)(conv_t<Args>...);
    };

    template <uint16_t N>
    class static_string {
    public:
        constexpr explicit static_string(std::string_view str) noexcept : static_string{str, std::make_integer_sequence<std::uint16_t, N>{}} {}
        constexpr const char* data() const noexcept { return chars_; }
        constexpr std::uint16_t size() const noexcept { return N; }
    private:
        template <uint16_t... I>
        constexpr static_string(std::string_view str, std::integer_sequence<std::uint16_t, I...>) noexcept : chars_{str[I]..., '\0'} {}
        char chars_[static_cast<size_t>(N) + 1];
    };
    template <>
    class static_string<0> {
    public:
        constexpr explicit static_string() = default;
        constexpr explicit static_string(std::string_view) noexcept {}
        constexpr const char* data() const noexcept { return nullptr; }
        constexpr uint16_t size() const noexcept { return 0; }
    };

    template <auto F>
    constexpr auto symbol() noexcept {
        std::string_view name =
#if defined(_MSC_VER)
            {__FUNCSIG__, sizeof(__FUNCSIG__)}
#else
            {__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__)}
#endif
        ;
#define STRING_FIND(C)                         \
        do {                                   \
            i = name.find(C, i);               \
            if (i == std::string_view::npos) { \
                return std::string_view {};    \
            }                                  \
            ++i;                               \
        } while (0)

        std::size_t i = 0;
#if defined(_MSC_VER)
        STRING_FIND('<');        
        i = name.find("lua", i);
        name.remove_prefix(i);
        i = name.find('(');
#else
        STRING_FIND('[');
        STRING_FIND('=');
        i = name.find("lua", i);
        name.remove_prefix(i);
        i = name.find(']');
#endif
        if (i == std::string_view::npos) {
            return std::string_view {};
        }
        name.remove_suffix(name.size() - i);
#undef STRING_FIND
        return name;
    }

    template <auto F>
    constexpr auto symbol_string() noexcept {
        constexpr auto name = symbol<F>();
        return static_string<name.size()>{name};
    }

    template <typename T>
    struct global { 
        static inline T v = T();
    };
    using initfunc = global<std::vector<std::function<const char* (resolver&)>>>;
    template <auto F>
    struct function {
        using type_t = function<F>;
        using func_t = typename invocable<decltype(F)>::type;
        static constexpr auto symbol_name = symbol_string<F>();
        static const char* init(resolver& r);
        static inline func_t invoke = []()->func_t{
            initfunc::v.push_back(type_t::init);
            return nullptr;
        }();
    };
    template <auto F>
    const char* function<F>::init(resolver& r) {
        type_t::invoke = reinterpret_cast<decltype(type_t::invoke)>(r.find(std::string_view { symbol_name.data(), symbol_name.size() }));
        return type_t::invoke != nullptr ? nullptr : symbol_name.data();
    }
    template <> struct conv<lua_State*>    { using type = state; };
    template <> struct conv<lua_CFunction> { using type = cfunction; };
    template <> struct conv<lua_KContext>  { using type = kcontext; };
    template <> struct conv<lua_KFunction> { using type = kfunction; };
    template <> struct conv<lua_Debug*>    { using type = debug; };
    template <> struct conv<lua_Hook>      { using type = hook; };
    
}

namespace luadebug::lua {
    inline const char* initialize(resolver& r) {
        for (auto& f : impl::initfunc::v) {
            auto error_name = f(r);
            if (error_name)
                return error_name;
        }
        return nullptr;
    }

    template <auto F, typename ...Args>
    auto call(Args... args) {
        return impl::function<F>::invoke(std::forward<Args>(args)...);
    }

    inline void pop(state L, int n) {
        call<lua_settop>(L, -(n)-1);
    }
    inline int pcall(state L, int nargs, int nresults, int errfunc) {
        return call<lua_pcallk>(L, nargs, nresults, errfunc, 0, nullptr);
    }
    inline int loadbuffer(state L, const char *buff, size_t sz, const char *name) {
        return call<luaL_loadbufferx>(L, buff, sz, name, nullptr);
    }
    inline const char* tostring(state L, int idx) {
        return call<lua_tolstring>(L, idx, nullptr);
    }
}
