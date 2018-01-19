#pragma once

#include <string>
#include <vector>
#include <regex>
#include "dbg_enum.h" 
#include "dbg_path.h"

namespace vscode
{
	class custom
	{
	public:
		typedef std::vector<std::pair<std::regex, std::string>> sourcemap_t;

		virtual bool path_convert(const std::string& server_path, fs::path& client_path)
		{
			return false;
		}

		virtual void set_state(state state)
		{ }

		virtual void update_stop()
		{ }
	};
}
