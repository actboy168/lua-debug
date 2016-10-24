#pragma once

#include <filesystem>

namespace fs = std::tr2::sys;

namespace vscode
{
	fs::path path_normalize(const fs::path& path);
	fs::path path_uncomplete(const fs::path& path, const fs::path& base, std::error_code& ec);
	bool     path_is_subpath(const fs::path& path, const fs::path& base);
}
