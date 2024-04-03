#include <bee/lua/binding.h>
#include <binding/binding.h>
#if defined _WIN32
#    include <winsock.h>

#    include <map>
#else
#    include <sys/select.h>
#endif
#include <bee/error.h>
#include <bee/net/socket.h>
#include <bee/thread/simplethread.h>

namespace bee::lua_socketlegacy {
    static int push_neterror(lua_State* L, std::string_view msg) {
        auto error = make_neterror(msg);
        lua_pushnil(L);
        lua_pushstring(L, error.c_str());
        return 2;
    }
    static net::fd_t checkfd(lua_State* L, int idx) {
        net::fd_t fd = lua::checkudata<net::fd_t>(L, idx);
        if (fd == net::retired_fd) {
            luaL_error(L, "socket is already closed.");
        }
        return fd;
    }
#if defined(_WIN32)
    struct socket_set {
        struct storage {
            uint32_t count;
            SOCKET array[1];
        };
        socket_set()
            : instack()
            , inheap(nullptr) {
            FD_ZERO(&instack);
        }
        socket_set(uint32_t n) {
            if (n > FD_SETSIZE) {
                inheap        = reinterpret_cast<storage*>(new uint8_t[sizeof(uint32_t) + sizeof(SOCKET) * n]);
                inheap->count = n;
            }
            else {
                FD_ZERO(&instack);
                inheap = nullptr;
            }
        }
        ~socket_set() {
            delete[] (reinterpret_cast<uint8_t*>(inheap));
        }
        fd_set* operator&() {
            if (inheap) {
                return reinterpret_cast<fd_set*>(inheap);
            }
            return &instack;
        }
        void set(SOCKET fd, lua_Integer i) {
            fd_set* set                    = &*this;
            set->fd_array[set->fd_count++] = fd;
            map[fd]                        = i;
        }
        fd_set instack;
        storage* inheap;
        std::map<SOCKET, lua_Integer> map;
    };

    static int select(lua_State* L) {
        bool read_finish = true, write_finish = true;
        if (lua_type(L, 1) == LUA_TTABLE)
            read_finish = false;
        else if (!lua_isnoneornil(L, 1))
            luaL_typeerror(L, 1, lua_typename(L, LUA_TTABLE));
        if (lua_type(L, 2) == LUA_TTABLE)
            write_finish = false;
        else if (!lua_isnoneornil(L, 2))
            luaL_typeerror(L, 2, lua_typename(L, LUA_TTABLE));
        int msec = lua::optinteger<int, -1>(L, 3);
        if (read_finish && write_finish) {
            if (msec < 0) {
                return luaL_error(L, "no open sockets to check and no timeout set");
            }
            else {
                thread_sleep(msec);
                lua_newtable(L);
                lua_newtable(L);
                return 2;
            }
        }
        struct timeval timeout, *timeop = &timeout;
        if (msec < 0) {
            timeop = NULL;
        }
        else {
            timeout.tv_sec  = (long)(msec / 1000);
            timeout.tv_usec = (long)(msec % 1000 * 1000);
        }
        lua_settop(L, 3);
        lua_Integer read_n  = read_finish ? 0 : luaL_len(L, 1);
        lua_Integer write_n = write_finish ? 0 : luaL_len(L, 2);
        socket_set readfds(static_cast<uint32_t>(read_n));
        socket_set writefds(static_cast<uint32_t>(write_n));
        for (lua_Integer i = 1; i <= read_n; ++i) {
            lua_rawgeti(L, 1, i);
            readfds.set(checkfd(L, -1), i);
            lua_pop(L, 1);
        }
        for (lua_Integer i = 1; i <= write_n; ++i) {
            lua_rawgeti(L, 2, i);
            writefds.set(checkfd(L, -1), i);
            lua_pop(L, 1);
        }
        int ok = ::select(0, &readfds, &writefds, &writefds, timeop);
        if (ok < 0) {
            return push_neterror(L, "select");
        }
        else if (ok == 0) {
            lua_newtable(L);
            lua_newtable(L);
            return 2;
        }
        else {
            {
                lua_newtable(L);
                auto set        = &readfds;
                lua_Integer out = 0;
                for (uint32_t i = 0; i < set->fd_count; ++i) {
                    SOCKET fd         = set->fd_array[i];
                    lua_Integer index = readfds.map[fd];
                    lua_rawgeti(L, 1, index);
                    lua_rawseti(L, 4, ++out);
                }
            }
            {
                lua_newtable(L);
                auto set        = &writefds;
                lua_Integer out = 0;
                for (uint32_t i = 0; i < set->fd_count; ++i) {
                    SOCKET fd         = set->fd_array[i];
                    lua_Integer index = writefds.map[fd];
                    lua_rawgeti(L, 2, index);
                    lua_rawseti(L, 5, ++out);
                }
            }
            return 2;
        }
    }
#else
    static int select(lua_State* L) {
        bool read_finish = true, write_finish = true;
        if (lua_type(L, 1) == LUA_TTABLE)
            read_finish = false;
        else if (!lua_isnoneornil(L, 1))
            luaL_typeerror(L, 1, lua_typename(L, LUA_TTABLE));
        if (lua_type(L, 2) == LUA_TTABLE)
            write_finish = false;
        else if (!lua_isnoneornil(L, 2))
            luaL_typeerror(L, 2, lua_typename(L, LUA_TTABLE));
        int msec = lua::optinteger<int, -1>(L, 3);
        if (read_finish && write_finish) {
            if (msec < 0) {
                return luaL_error(L, "no open sockets to check and no timeout set");
            }
            else {
                thread_sleep(msec);
                lua_newtable(L);
                lua_newtable(L);
                return 2;
            }
        }
        struct timeval timeout, *timeop = &timeout;
        if (msec < 0) {
            timeop = NULL;
        }
        else {
            timeout.tv_sec  = (long)(msec / 1000);
            timeout.tv_usec = (long)(msec % 1000 * 1000);
        }
        lua_settop(L, 3);
        int maxfd = 0;
        fd_set readfds;
        fd_set writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        lua_Integer read_n  = read_finish ? 0 : luaL_len(L, 1);
        lua_Integer write_n = write_finish ? 0 : luaL_len(L, 2);
        if (!read_finish) {
            for (lua_Integer i = 1; i <= read_n; ++i) {
                lua_rawgeti(L, 1, i);
                auto fd = checkfd(L, -1);
                FD_SET(fd, &readfds);
                maxfd = std::max(maxfd, fd);
                lua_pop(L, 1);
            }
        }
        if (!write_finish) {
            for (lua_Integer i = 1; i <= write_n; ++i) {
                lua_rawgeti(L, 2, i);
                auto fd = checkfd(L, -1);
                FD_SET(fd, &writefds);
                maxfd = std::max(maxfd, fd);
                lua_pop(L, 1);
            }
        }
        int ok;
        if (timeop == NULL) {
            do
                ok = ::select(maxfd + 1, &readfds, &writefds, NULL, NULL);
            while (ok == -1 && errno == EINTR);
        }
        else {
            ok = ::select(maxfd + 1, &readfds, &writefds, NULL, timeop);
            if (ok == -1 && errno == EINTR) {
                lua_newtable(L);
                lua_newtable(L);
                return 2;
            }
        }
        if (ok < 0) {
            return push_neterror(L, "select");
        }
        else if (ok == 0) {
            lua_newtable(L);
            lua_newtable(L);
            return 2;
        }
        else {
            lua_Integer rout = 0, wout = 0;
            lua_newtable(L);
            lua_newtable(L);
            for (lua_Integer i = 1; i <= read_n; ++i) {
                lua_rawgeti(L, 1, i);
                if (FD_ISSET(lua::toudata<net::fd_t>(L, -1), &readfds)) {
                    lua_rawseti(L, 4, ++rout);
                }
                else {
                    lua_pop(L, 1);
                }
            }
            for (lua_Integer i = 1; i <= write_n; ++i) {
                lua_rawgeti(L, 2, i);
                if (FD_ISSET(lua::toudata<net::fd_t>(L, -1), &writefds)) {
                    lua_rawseti(L, 5, ++wout);
                }
                else {
                    lua_pop(L, 1);
                }
            }
            return 2;
        }
    }
#endif
    static int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            { "select", select },
            { NULL, NULL }
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(socketlegacy)
