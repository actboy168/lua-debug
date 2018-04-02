#pragma once

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
	struct io;
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

	enum class state {
		birth = 1,
		initialized = 2,
		stepping = 3,
		running = 4,
		terminated = 5,
	};

	struct custom
	{
		virtual bool path_convert(const std::string& source, std::string& path) = 0;
	};

	class DEBUGGER_API debugger
	{
	public:
		debugger(io* io, threadmode mode);
		~debugger();
		void close();
		void update();
		void wait_attach();
		void attach_lua(lua_State* L);
		void detach_lua(lua_State* L);
		void set_custom(custom* custom);
		void output(const char* category, const char* buf, size_t len, lua_State* L = nullptr);
		void exception(lua_State* L);
		bool is_state(state state) const;
		void redirect_stdout();
		void redirect_stderr();
		bool set_config(int level, const std::string& cfg, std::string& err);

	private:
		debugger_impl* impl_;
	};
}

extern "C" {
	DEBUGGER_API int  __cdecl luaopen_debugger(lua_State* L);
	DEBUGGER_API void __cdecl debugger_set_luadll(void* luadll, void* getluaapi);
}
