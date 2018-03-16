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

	enum class coding
	{
		ansi,
		utf8,
	};

	class DEBUGGER_API debugger
	{
	public:
		debugger(io* io, threadmode mode, coding coding);
		~debugger();
		void update();
		void attach_lua(lua_State* L, bool pause);
		void detach_lua(lua_State* L);
		void set_custom(custom* custom);
		void set_coding(coding coding);
		void output(const char* category, const char* buf, size_t len);

	private:
		debugger_impl* impl_;
	};
}

extern "C" {
	DEBUGGER_API int  __cdecl luaopen_debugger(lua_State* L);
	DEBUGGER_API void __cdecl debugger_set_luadll(void* luadll);
	DEBUGGER_API void __cdecl debugger_set_coding(int coding);
	DEBUGGER_API void __cdecl debugger_start_server(const char* ip, uint16_t port, bool launch, bool rebind);
	DEBUGGER_API void __cdecl debugger_attach_lua(lua_State* L, bool pause);
	DEBUGGER_API void __cdecl debugger_detach_lua(lua_State* L);
}
