#pragma once

#include <filesystem>

namespace fs = std::tr2::sys;

namespace vscode
{
	fs::path path_uncomplete(const fs::path& p, const fs::path& base);
}
