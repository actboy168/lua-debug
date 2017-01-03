#pragma once

#include <filesystem>

#if _MSC_VER >= 1900
namespace fs = std::experimental::filesystem::v1;
#define path_to_string(x) (x).string()
#else
namespace fs_ = std::tr2::sys;

namespace fs {
	class path
		: public fs_::path
	{
	public:
		path()
			: fs_::path()
		{ }

		path(const char* str)
			: fs_::path(str)
		{ }

		path(const std::string& str)
			: fs_::path(str)
		{ }

		path(const path& p)
			: fs_::path(p)
		{ }

		path(const fs_::path& p)
			: fs_::path(p)
		{ }

		path(std::string&& str)
			: fs_::path(std::forward<std::string>(str))
		{ }

		path(path&& p)
			: fs_::path(std::forward<path>(p))
		{ }

		path(fs_::path&& p)
			: fs_::path(std::forward<fs_::path>(p))
		{ }

		bool is_absolute() const
		{
			return fs_::path::is_complete();
		}
	};

	inline path absolute(const path& p, const path& base)
	{
		return fs_::complete(p, base);
	}  

	inline path current_path()
	{ 
		return fs_::current_path<path>();
	}

	inline void current_path(const path& p)
	{
		fs_::current_path(p);
	}
}
#define path_to_string(x) (x)
#endif

namespace vscode
{
	fs::path path_normalize(const fs::path& path);
	fs::path path_uncomplete(const fs::path& path, const fs::path& base, std::error_code& ec);
	bool     path_is_subpath(const fs::path& path, const fs::path& base);
}
