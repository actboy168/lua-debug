#include "common.hpp"
namespace autoattach {
	lua_version lua_version_from_string(const std::string_view& v) {
		if (v == "luajit")
			return lua_version::luajit;
		if (v == "lua51")
			return lua_version::lua51;
		if (v == "lua52")
			return lua_version::lua52;
		if (v == "lua53")
			return lua_version::lua53;
		if (v == "lua54")
			return lua_version::lua54;
		return lua_version::unknown;
	}

	const char* lua_version_to_string(lua_version v) {
		switch (v) {
		case lua_version::lua51:
			return "lua51";
		case lua_version::lua52:
			return "lua52";
		case lua_version::lua53:
			return "lua53";
		case lua_version::lua54:
			return "lua54";		
		case lua_version::luajit:
			return "luajit";
		default:
			return "unknown";
		}
	}
}