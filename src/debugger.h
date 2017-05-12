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
		void attach_lua(lua_State* L, bool pause);
		void detach_lua(lua_State* L);
		void set_custom(custom* custom);
		void output(const char* category, const char* buf, size_t len);

	private:
		debugger_impl* impl_;
	};
}

extern "C" {
	DEBUGGER_API int __cdecl luaopen_debugger(lua_State* L);
	DEBUGGER_API void set_luadll(const char* path, size_t len);
	DEBUGGER_API void start_server(const char* ip, uint16_t port);
	DEBUGGER_API void attach_lua(lua_State* L, bool pause);
}
