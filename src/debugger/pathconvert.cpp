#include <debugger/pathconvert.h>
#include <debugger/impl.h>
#include <debugger/path.h>
#include <base/util/unicode.h>
#include <base/util/dynarray.h>
#include <deque>
#include <Windows.h>

namespace vscode
{
	pathconvert::pathconvert(debugger_impl* dbg)
		: debugger_(dbg)
		, sourcemaps_()
		, coding_(coding::ansi)
	{ }

	void pathconvert::set_coding(coding coding)
	{
		coding_ = coding;
		source2path_.clear();
	}

	void pathconvert::add_sourcemap(const std::string& srv, const std::string& cli)
	{
		sourcemaps_.push_back(std::make_pair(srv, cli));
	}

	void pathconvert::clear_sourcemap()
	{
		sourcemaps_.clear();
	}

	bool pathconvert::match_sourcemap(const std::string& srv, std::string& cli, const std::string& srvmatch, const std::string& climatch)
	{
		size_t i = 0;
		for (; i < srvmatch.size(); ++i) {
			if (i >= srv.size()) {
				return false;
			}
			if (path::tochar(srvmatch[i]) == path::tochar(srv[i])) {
				continue;
			}
			return false;
		}
		cli = climatch + srv.substr(i);
		return true;
	}

	std::string pathconvert::find_sourcemap(const std::string& srv)
	{
		std::string cli;
		for (auto& it : sourcemaps_)
		{
			if (match_sourcemap(srv, cli, it.first, it.second))
			{
				return cli;
			}
		}
		return path::normalize(srv);
	}

	bool pathconvert::get(const std::string& source, std::string& client_path)
	{
		auto it = source2path_.find(source);
		if (it != source2path_.end())
		{
			client_path = it->second;
			return !client_path.empty();
		}

		bool res = true;
		if (debugger_->custom_) {
			std::string path;
			if (debugger_->custom_->path_convert(source, path)) {
				client_path = find_sourcemap(path);
			}
			else {
				client_path.clear();
				res = false;
			}
		}
		else {
			if (source[0] == '@') {
				if (coding_ == coding::utf8) {
					client_path = find_sourcemap(source.substr(1));
				}
				else {
					client_path = find_sourcemap(base::a2u(base::strview(source.data() + 1, source.size() - 1)));
				}
			}
			else {
				client_path.clear();
				res = false;
			}
		}
		source2path_[source] = client_path;
		return res;
	}
}
