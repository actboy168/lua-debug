#pragma once

#include <string>

namespace vscode { namespace io {
	struct base {
		virtual void        update(int ms) = 0;
		virtual bool        output(const char* buf, size_t len) = 0;
		virtual bool        input(std::string& buf) = 0;
		virtual void        close() = 0;
	};
}}
