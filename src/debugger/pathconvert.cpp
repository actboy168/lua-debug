#include <debugger/pathconvert.h>
#include <debugger/impl.h>
#include <debugger/path.h>
#include <base/util/unicode.h>
#include <base/util/dynarray.h>
#include <deque>
#include <regex>
#include <Windows.h>

namespace vscode
{
	pathconvert::pathconvert(debugger_impl* dbg)
		: debugger_(dbg)
		, sourcemap_()
		, coding_(eCoding::ansi)
	{ }

	void pathconvert::set_coding(eCoding coding)
	{
		coding_ = coding;
		source2client_.clear();
	}

	void pathconvert::add_sourcemap(const std::string& server, const std::string& client)
	{
		sourcemap_.push_back(std::make_pair(server, client));
	}

	void pathconvert::add_skipfiles(const std::string& pattern)
	{
		skipfiles_.push_back(pattern);
	}

	void pathconvert::clear()
	{
		sourcemap_.clear();
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

	bool pathconvert::server2client(const std::string& server, std::string& client)
	{
		for (auto& it : skipfiles_)
		{
			if (path::glob_match(it, server))
			{
				return false;
			}
		}

		for (auto& it : sourcemap_)
		{
			if (match_sourcemap(server, client, it.first, it.second))
			{
				return true;
			}
		}
		client = path::normalize(server);
		return true;
	}

	bool pathconvert::get(const std::string& source, std::string& client)
	{
		auto it = source2client_.find(source);
		if (it != source2client_.end())
		{
			client = it->second;
			return !client.empty();
		}

		bool res = true;
		if (debugger_->custom_) {
			std::string server;
			if (debugger_->custom_->path_convert(source, server)) {
				res = server2client(server, client);
			}
			else {
				client.clear();
				res = false;
			}
		}
		else {
			if (source[0] == '@') {
				std::string server = coding_ == eCoding::utf8
					? source.substr(1) 
					: base::a2u(base::strview(source.data() + 1, source.size() - 1))
					;
				res = server2client(server, client);
			}
			else {
				client.clear();
				res = false;
			}
		}
		source2client_[source] = client;
		return res;
	}

	std::string pathconvert::exception(const std::string& str)
	{
		if (coding_ == eCoding::utf8) {
			return str;
		}
		std::regex re(R"(([^\r\n]+)(\:[0-9]+\:[^\r\n]+))");
		std::string res;
		std::smatch m;
		auto it = str.begin();
		for (; std::regex_search(it, str.end(), m, re); it = m[0].second) {
			res += std::string(it, m[0].first);
			res += base::a2u(std::string(m[1].first, m[1].second));
			res += std::string(m[2].first, m[2].second);
		}
		res += std::string(it, str.end());
		return res;
	}
}
