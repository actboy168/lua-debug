#pragma once

struct lua_State;
struct lua_Debug;

namespace vscode
{
	bool evaluate(lua_State* L, lua_Debug *ar, const char* script, int& nresult, bool writeable = false);
}
