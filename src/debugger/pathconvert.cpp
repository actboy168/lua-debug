#include <debugger/impl.h>
#include <debugger/path.h>
#include <base/util/unicode.h>
#include <base/util/dynarray.h>
#include <deque>
#include <regex>

namespace vscode
{
	static bool match_sourcemap(const std::string& srv, std::string& cli, const std::string& srvmatch, const std::string& climatch)
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

	void debugger_impl::initialize_pathconvert(config& config)
	{
		sourceMap_.clear();
		auto& sourceMaps = config.get("sourceMaps", rapidjson::kArrayType);
		for (auto& e : sourceMaps.GetArray())
		{
			if (!e.IsArray())
			{
				continue;
			}
			auto eary = e.GetArray();
			if (eary.Size() < 2 || !eary[0].IsString() || !eary[1].IsString())
			{
				continue;
			}
			sourceMap_.push_back(std::make_pair(eary[0].Get<std::string>(), eary[1].Get<std::string>()));
		}
		auto& skipFiles = config.get("skipFiles", rapidjson::kArrayType);
		for (auto& e : skipFiles.GetArray())
		{
			if (!e.IsString())
			{
				continue;
			}
			skipFiles_.push_back(e.Get<std::string>());
		}
		workspaceFolder_ = config.get("workspaceFolder", rapidjson::kStringType).Get<std::string>();
	}

	bool debugger_impl::path_server2client(const std::string& server, std::string& client)
	{
		for (auto& it : skipFiles_)
		{
			if (path::glob_match(it, server))
			{
				return false;
			}
		}

		for (auto& it : sourceMap_)
		{
			if (match_sourcemap(server, client, it.first, it.second))
			{
				return true;
			}
		}
		client = path::normalize(server, '/');
		return true;
	}

	bool debugger_impl::path_source2server(const std::string& source, std::string& server)
	{
		if (custom_) {
			return custom_->path_convert(source, server);
		}
		if (source[0] != '@') {
			return false;
		}
		server = sourceCoding_ == eCoding::utf8
			? source.substr(1)
			: base::a2u(base::strview(source.data() + 1, source.size() - 1))
			;
		return true;
	}

	bool debugger_impl::path_source2client(const std::string& source, std::string& client)
	{
		std::string server;
		return path_source2server(source, server) && path_server2client(server, client);
	}

	bool debugger_impl::path_convert(const std::string& source, std::string& client)
	{
		auto it = source2client_.find(source);
		if (it != source2client_.end()) {
			client = it->second;
			return !client.empty();
		}
		bool ok = path_source2client(source, client);
		source2client_[source] = client;
		return ok;
	}

	std::string debugger_impl::path_exception(const std::string& str)
	{
		if (sourceCoding_ == eCoding::utf8) {
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

	std::string debugger_impl::path_clientrelative(const std::string& path) {
		if (workspaceFolder_.empty()) {
			return path;
		}
		return path::relative(path, workspaceFolder_, '/');
	}
}
