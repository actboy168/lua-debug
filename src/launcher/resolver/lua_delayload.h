#pragma once

#include <resolver/lua_funcname.h>
#include <stdint.h>

#include <functional>
#include <lua.hpp>
#include <string_view>
#include <type_traits>
#include <vector>

namespace luadebug::lua {
    using state     = uintptr_t;
    using cfunction = int (*)(state);
    using kcontext  = LUA_KCONTEXT;
    using kfunction = int (*)(state, int, kcontext);
    using debug     = uintptr_t;
    using hook      = void (*)(state, debug);

    struct resolver {
        virtual intptr_t find(std::string_view name) const = 0;
    };
}

namespace luadebug::lua::impl {
    template <class T>
    struct conv {
        using type = T;
    };
    template <class T>
    using conv_t = typename conv<T>::type;
    template <class Sig>
    struct invocable;
    template <class R, class... Args>
    struct invocable<R(Args...)> {
        using type = conv_t<R> (*)(conv_t<Args>...);
    };
    template <class R, class... Args>
    struct invocable<R (*)(Args...)> {
        using type = conv_t<R> (*)(conv_t<Args>...);
    };

    template <typename T>
    struct global {
        static inline T v = T();
    };
    using initfunc = global<std::vector<std::function<const char*(resolver&)>>>;
    template <auto F>
    struct function {
        using type_t = function<F>;
        using func_t = typename invocable<decltype(F)>::type;
        static const char* init(resolver& r);
        static inline func_t invoke = []() -> func_t {
            initfunc::v.push_back(type_t::init);
            return nullptr;
        }();
    };
    template <auto F>
    const char* function<F>::init(resolver& r) {
        type_t::invoke = reinterpret_cast<decltype(type_t::invoke)>(r.find(function_name_v<F>));
        return type_t::invoke != nullptr ? nullptr : function_name_v<F>.data();
    }
    template <>
    struct conv<lua_State*> {
        using type = state;
    };
    template <>
    struct conv<lua_CFunction> {
        using type = cfunction;
    };
    template <>
    struct conv<lua_KContext> {
        using type = kcontext;
    };
    template <>
    struct conv<lua_KFunction> {
        using type = kfunction;
    };
    template <>
    struct conv<lua_Debug*> {
        using type = debug;
    };
    template <>
    struct conv<lua_Hook> {
        using type = hook;
    };

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

    template <auto F, typename... Args>
    auto call(Args... args) {
        return impl::function<F>::invoke(std::forward<Args>(args)...);
    }

    inline void pop(state L, int n) {
        call<lua_settop>(L, -(n)-1);
    }
    inline int pcall(state L, int nargs, int nresults, int errfunc) {
        return call<lua_pcallk>(L, nargs, nresults, errfunc, 0, nullptr);
    }
    inline int loadbuffer(state L, const char* buff, size_t sz, const char* name) {
        return call<luaL_loadbufferx>(L, buff, sz, name, nullptr);
    }
    inline const char* tostring(state L, int idx) {
        return call<lua_tolstring>(L, idx, nullptr);
    }
}
