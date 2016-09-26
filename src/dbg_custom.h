#pragma once

#include <string>
#include "dbg_enum.h"

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

		virtual result path_convert(const std::string& server_path, std::string& client_path)
		{
			return result::failed;
		}

		virtual void set_state(state state)
		{ }

		virtual void update_stop()
		{ }
	};
}
