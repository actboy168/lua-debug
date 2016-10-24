#pragma once

#include <string>
#include <vector>
#include "dbg_enum.h" 
#include "dbg_path.h"

namespace vscode
{
	class custom
	{
	public:
		enum class result
		{
			failed = 0,
			sucess = 1,
			failed_once = 2,
			sucess_once = 3,
		};
		typedef std::vector<std::pair<fs::path, fs::path>> sourcemap_t;

		virtual result path_convert(const std::string& server_path, fs::path& client_path, const sourcemap_t& map, bool uncomplete)
		{
			return result::failed;
		}

		virtual void set_state(state state)
		{ }

		virtual void update_stop()
		{ }
	};
}
