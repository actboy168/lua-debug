#pragma once

struct lua_State;
namespace lua { union Debug; }

namespace vscode
{
	bool evaluate(lua_State* L, lua::Debug *ar, const char* script, int& nresult, bool writeable = false);
}
