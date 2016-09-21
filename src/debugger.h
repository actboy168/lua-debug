#pragma once

#include <memory>
#include <string>


#if defined(DEBUGGER_INLINE)
#	define DEBUGGER_API
#else
#	if defined(DEBUGGER_EXPORTS)
#		define DEBUGGER_API __declspec(dllexport)
#	else
#		define DEBUGGER_API __declspec(dllimport)
#	endif
#endif

struct lua_State;

namespace vscode
{
	class debugger_impl;

	class DEBUGGER_API debugger
	{
	public:
		debugger(lua_State* L, const char* ip, uint16_t port);
		~debugger();
		void update();
		void set_schema(const char* file);

	private:
		debugger_impl* impl_;
	};
}

extern "C" {
	DEBUGGER_API void* __cdecl vscode_debugger_create(lua_State* L, const char* ip, uint16_t port);
	DEBUGGER_API void  __cdecl vscode_debugger_close(void* dbg);
	DEBUGGER_API void  __cdecl vscode_debugger_set_schema(void* dbg, const char* file);
	DEBUGGER_API void  __cdecl vscode_debugger_update(void* dbg);
}
