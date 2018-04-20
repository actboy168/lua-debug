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
struct lua_Debug;

namespace vscode
{
	namespace io {
		struct base;
	};
	class debugger_impl;

	enum class eCoding
	{
		none,
		ansi,
		utf8,
	};

	enum class eState {
		birth = 1,
		initialized = 2,
		stepping = 3,
		running = 4,
		terminated = 5,
	};

	enum class eRedirect {
		stderror = 1,
		stdoutput = 2,
		print = 3,
	};

	struct custom
	{
		virtual bool path_convert(const std::string& source, std::string& path) = 0;
	};

	class DEBUGGER_API debugger
	{
	public:
		debugger(io::base* io);
		~debugger();
		bool open_schema(const std::wstring& path);
		void close();
		void update();
		void wait_attach();
		void attach_lua(lua_State* L);
		void detach_lua(lua_State* L, bool remove = false);
		void set_custom(custom* custom);
		void output(const char* category, const char* buf, size_t len, lua_State* L = nullptr, lua_Debug* ar = nullptr);
		void exception(lua_State* L);
		bool is_state(eState state) const;
		void open_redirect(eRedirect type, lua_State* L = nullptr);
		bool set_config(int level, const std::string& cfg, std::string& err);

	private:
		debugger_impl* impl_;
	};
}

extern "C" {
	DEBUGGER_API int  __cdecl luaopen_debugger(lua_State* L);
	DEBUGGER_API void __cdecl debugger_set_luadll(void* luadll, void* getluaapi);
}
