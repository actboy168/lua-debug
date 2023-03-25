#include <dlfcn.h>

#include <lua.hpp>
#include <string>
#include <thread>
using namespace std::literals;

int main(int argc, char** argv) {
    if (argc != 3) {
        return -1;
    }
    std::string error;
    auto launcher = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
    if (!launcher) {
        printf("Error: %s", dlerror());
        return 1;
    }
    auto attach = (void (*)())dlsym(launcher, "attach");
    attach();
    auto hmod = dlopen(argv[2], RTLD_NOW | RTLD_GLOBAL);
    if (!hmod) {
        printf("Error: %s", dlerror());
        return 1;
    }
    std::this_thread::sleep_for(1s);
    auto _luaL_newstate   = (decltype(&luaL_newstate))dlsym(hmod, "luaL_newstate");
    auto L                = _luaL_newstate();
    auto _lua_createtable = (decltype(&lua_createtable))dlsym(hmod, "lua_createtable");
    _lua_createtable(L, 0, 0);
    auto _lua_getfield = (decltype(&lua_getfield))dlsym(hmod, "lua_getfield");
    _lua_getfield(L, -1, "1");
    auto _lua_gethook = (decltype(&lua_gethook))dlsym(hmod, "lua_gethook");
    return _lua_gethook(L) != nullptr ? 0 : 3;
}