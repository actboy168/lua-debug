#pragma once

#include <base/filesystem.h>

namespace base{ namespace path {
	fs::path normalize(const fs::path& path);
}}
