#include "lua_delayload.h"
#include <string_view>
#include <cstdio>
#include <cassert>
int main(){
    using namespace std::string_view_literals;
#define check_symbol_name(name) \
    constexpr auto name##_s = lua_delayload::impl::symbol_string<name>();\
    static_assert((std::string_view{name##_s.data(), name##_s.size()} == #name##sv));

    check_symbol_name(lua_gethook);
    check_symbol_name(lua_sethook);

    assert(lua_delayload::impl::initfunc::v.size() == 4); 
	return 0;
}
