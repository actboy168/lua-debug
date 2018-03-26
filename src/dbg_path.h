#pragma once

#include <filesystem>

#if _MSC_VER >= 1900
namespace fs = std::experimental::filesystem::v1;
#else
namespace fs_ = std::tr2::sys;

namespace fs {
	class path
		: public fs_::wpath
	{
	public:
		path()
			: fs_::wpath()
		{ }

		path(const path& p)
			: fs_::wpath(p)
		{ }

		path(const fs_::wpath& p)
			: fs_::wpath(p)
		{ }

		path(path&& p)
			: fs_::wpath(std::forward<path>(p))
		{ }

		path(fs_::wpath&& p)
			: fs_::wpath(std::forward<fs_::wpath>(p))
		{ }

		bool is_absolute() const
		{
			return fs_::wpath::is_complete();
		}

		std::wstring wstring() const
		{
			return fs_::wpath::string();
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
#endif

namespace vscode
{
	fs::path path_normalize(const fs::path& path);
}
