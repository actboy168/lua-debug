#include "dbg_path.h"
#include <algorithm>

namespace vscode
{
	fs::path path_uncomplete(const fs::path& p, const fs::path& base)
	{
		if (p == base)
			return "./";
		fs::path from_path, from_base, output;
		fs::path::iterator path_it = p.begin(), path_end = p.end();
		fs::path::iterator base_it = base.begin(), base_end = base.end();

		if ((path_it == path_end) || (base_it == base_end))
			throw std::runtime_error("path or base was empty; couldn't generate relative path");

#ifdef WIN32
		if (*path_it != *base_it)
			return p;
		++path_it, ++base_it;
#endif

		const std::string _dot = std::string(1, fs::dot<fs::path>::value);
		const std::string _dots = std::string(2, fs::dot<fs::path>::value);
		const std::string _sep = std::string(1, fs::slash<fs::path>::value);

		while (true) {
			if ((path_it == path_end) || (base_it == base_end) || (*path_it != *base_it)) {
				for (; base_it != base_end; ++base_it) {
					if (*base_it == _dot)
						continue;
					else if (*base_it == _sep)
						continue;

					output /= "../";
				}
				fs::path::iterator path_it_start = path_it;
				for (; path_it != path_end; ++path_it) {

					if (path_it != path_it_start)
						output /= "/";

					if (*path_it == _dot)
						continue;
					if (*path_it == _sep)
						continue;

					output /= *path_it;
				}

				break;
			}
			from_path /= fs::path(*path_it);
			from_base /= fs::path(*base_it);
			++path_it, ++base_it;
		}

		return output;
	}
}
