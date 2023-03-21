#pragma once

#include <array>
#include <type_traits>
#include <variant>

struct lua_State;
struct luadbg_State;

namespace luadebug::refvalue {
    struct FRAME_LOCAL {
        uint16_t frame;
        int16_t n;
    };
    struct FRAME_FUNC {
        uint16_t frame;
    };
    struct GLOBAL {};
    enum class REGISTRY_TYPE {
        REGISTRY,
        DEBUG_REF,
        DEBUG_WATCH,
    };
    struct REGISTRY {
        REGISTRY_TYPE type;
    };
    struct STACK {
        int index;
    };
    struct UPVALUE {
        int n;
    };
    struct METATABLE {
        int type;
    };
    struct USERVALUE {
        int n;
    };
    struct TABLE_ARRAY {
        unsigned int index;
    };
    struct TABLE_HASH_KEY {
        unsigned int index;
    };
    struct TABLE_HASH_VAL {
        unsigned int index;
    };
    using value = std::variant<
        FRAME_LOCAL,
        FRAME_FUNC,
        GLOBAL,
        REGISTRY,
        STACK,
        UPVALUE,
        METATABLE,
        USERVALUE,
        TABLE_ARRAY,
        TABLE_HASH_KEY,
        TABLE_HASH_VAL>;

    static_assert(std::is_trivially_copyable_v<value>);
    int eval(value* v, lua_State* cL);
    bool assign(value* v, lua_State* cL);
    value* create_userdata(luadbg_State* L, int n);
    value* create_userdata(luadbg_State* L, int n, int parent);

    template <typename... Args>
    inline value* create(luadbg_State* L, int parent, Args&&... args) {
        static_assert(sizeof...(Args) > 0);
        using value_array = std::array<value, sizeof...(Args)>;
        auto v            = create_userdata(L, sizeof...(Args), parent);
        new (reinterpret_cast<value_array*>(v)) value_array { std::forward<Args>(args)... };
        return v;
    }

    template <typename... Args>
    inline value* create(luadbg_State* L, Args&&... args) {
        static_assert(sizeof...(Args) > 0);
        using value_array = std::array<value, sizeof...(Args)>;
        auto v            = create_userdata(L, sizeof...(Args));
        new (reinterpret_cast<value_array*>(v)) value_array { std::forward<Args>(args)... };
        return v;
    }
}
