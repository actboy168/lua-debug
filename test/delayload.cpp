#include "lua_delayload.h"
#include <string_view>
#include <cstdio>
int main(){
    using namespace std::string_view_literals;
    constexpr auto lua_sethook_s = lua_delayload::impl::symbol_string<lua_gethook>();
    static_assert(std::string_view{lua_sethook_s.data(), lua_sethook_s.size()} == "lua_gethook"sv);
	return 0;
}
