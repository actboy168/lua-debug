#pragma once

#include <lua.hpp>
#include <functional>
#include <type_traits>

namespace lua_delayload {
    using state = uintptr_t;
    using cfunction = int (*) (state);
    using kcontext = LUA_KCONTEXT;
    using kfunction = int (*) (state, int, kcontext);
    using debug = uintptr_t;
    using hook = void (*) (state, debug);

    struct resolver {
        virtual intptr_t find(const char* name) = 0;
    };
}

namespace lua_delayload::impl {
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
    template <auto F>
    struct symbol;
    template <typename T>
    struct global { static inline T v = T(); };
    using initfunc = global<std::vector<std::function<void(resolver&)>>>;
    template <auto F>
    struct function {
        static inline typename invocable<decltype(F)>::type invoke;
        static inline bool _ = ([](){
            initfunc::v.push_back([](resolver& r) {
                invoke = reinterpret_cast<decltype(function<F>::invoke)>(r.find(symbol<F>::name));
            });
            return true;
        })();
    };

    template <> struct conv<lua_State*>    { using type = state; };
    template <> struct conv<lua_CFunction> { using type = cfunction; };
    template <> struct conv<lua_KContext>  { using type = kcontext; };
    template <> struct conv<lua_KFunction> { using type = kfunction; };
    template <> struct conv<lua_Debug*>    { using type = debug; };
    template <> struct conv<lua_Hook>      { using type = hook; };
    template <> struct symbol<lua_settop>       { static inline const char name[] = "lua_settop"; };
    template <> struct symbol<lua_pcallk>       { static inline const char name[] = "lua_pcallk"; };
    template <> struct symbol<luaL_loadbufferx> { static inline const char name[] = "luaL_loadbufferx"; };
    template <> struct symbol<lua_pushstring>   { static inline const char name[] = "lua_pushstring"; };
    template <> struct symbol<lua_pushlstring>  { static inline const char name[] = "lua_pushlstring"; };
    template <> struct symbol<lua_tolstring>    { static inline const char name[] = "lua_tolstring"; };
    template <> struct symbol<lua_sethook>      { static inline const char name[] = "lua_sethook"; };
    template <> struct symbol<lua_gethook>      { static inline const char name[] = "lua_gethook"; };
    template <> struct symbol<lua_gethookmask>  { static inline const char name[] = "lua_gethookmask"; };
    template <> struct symbol<lua_gethookcount> { static inline const char name[] = "lua_gethookcount"; };
    
}

namespace lua_delayload {
    inline void initialize(resolver& r) {
        for (auto& f : impl::initfunc::v) {
            f(r);
        }
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

namespace lua = lua_delayload;
