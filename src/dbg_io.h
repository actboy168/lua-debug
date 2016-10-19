#pragma once

#include "dbg_protocol.h"

namespace vscode
{
	class io {
	public:
		virtual void      update(int ms) = 0;
		virtual bool      output(const wprotocol& wp) = 0;
		virtual rprotocol input() = 0;
		virtual bool      input_empty() const = 0;
		virtual void      close() = 0;
	};
}
