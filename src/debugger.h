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
	class io;
	class custom;
	class debugger_impl;

	enum class threadmode
	{
		async,
		sync,
	};

	class DEBUGGER_API debugger
	{
	public:
		debugger(io* io, threadmode mode);
		~debugger();
		void update();
		void attach_lua(lua_State* L);
		void detach_lua(lua_State* L);
		void set_custom(custom* custom);
		void output(const char* category, const char* buf, size_t len);

	private:
		debugger_impl* impl_;
	};
}

extern "C" {
#define DEBUGGER_THREADMODE_ASYNC 0
#define DEBUGGER_THREADMODE_SYNC  1
	DEBUGGER_API void* __cdecl vscode_debugger_create(const char* ip, uint16_t port, int threadmode);
	DEBUGGER_API void  __cdecl vscode_debugger_close(void* dbg);
	DEBUGGER_API void  __cdecl vscode_debugger_set_schema(void* dbg, const char* file);
	DEBUGGER_API void  __cdecl vscode_debugger_update(void* dbg);
	DEBUGGER_API void  __cdecl vscode_debugger_attach_lua(void* dbg, lua_State* L);
	DEBUGGER_API void  __cdecl vscode_debugger_detach_lua(void* dbg, lua_State* L);
}
