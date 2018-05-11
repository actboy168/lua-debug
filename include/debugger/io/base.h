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

namespace vscode { namespace io {
	typedef void (*CloseEvent)(void* userdata);
	struct DEBUGGER_API base {
		virtual void update(int ms) = 0;
		virtual bool output(const char* buf, size_t len) = 0;
		virtual bool input(std::string& buf) = 0;
		virtual void close() = 0;
		virtual void on_close_event(CloseEvent fn, void* ud) { };
	};
}}
