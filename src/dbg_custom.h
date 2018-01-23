#pragma once

#include <string>
#include "dbg_path.h"

namespace vscode
{
	class custom
	{
	public:

		virtual bool path_convert(const std::string& server_path, fs::path& client_path)
		{
			return false;
		}
	};
}
