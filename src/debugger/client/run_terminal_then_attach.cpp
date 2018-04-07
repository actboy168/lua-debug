#include <debugger/io/namedpipe.h>
#include <Windows.h>
#include <debugger/client/run.h>
#include <debugger/io/base.h>
#include <debugger/io/helper.h>
#include <debugger/client/stdinput.h>
#include <base/util/format.h>
#include <base/path/self.h>
#include <functional>

static void sleep() {
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void request_runInTerminal(vscode::io::base* io, std::function<void(vscode::wprotocol&)> args)
{
	vscode::wprotocol res;
	for (auto _ : res.Object())
	{
		res("type").String("request");
		//res("seq").Int64(seq++);
		res("command").String("runInTerminal");
		for (auto _ : res("arguments").Object())
		{
			args(res);
		}
	}
	vscode::io_output(io, res);
}

int run_terminal_then_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req)
{
	auto& args = req["arguments"];
	bool sourceCodingUtf8 = false;
	std::string sourceCoding = "ansi";
	if (args.HasMember("sourceCoding") && args["sourceCoding"].IsString()) {
		sourceCodingUtf8 = "utf8" == args["sourceCoding"].Get<std::string>();
	}
	auto pipename = base::format(L"vscode-lua-debug-%d", GetCurrentProcessId());

	request_runInTerminal(&io, [&](vscode::wprotocol& res) {
		res("kind").String(args["console"] == "integratedTerminal" ? "integrated" : "external");
		res("title").String("Lua Debug");
		if (args.HasMember("cwd") && args["cwd"].IsString()) {
			res("cwd").String(args["cwd"]);
		}
		if (args.HasMember("env") && args["env"].IsObject()) {
			for (auto _ : res("env").Object()) {
				for (auto& v : args["env"].GetObject()) {
					if (v.name.IsString()) {
						if (v.value.IsString()) {
							res(v.name).String(v.value);
						}
						else if (v.value.IsNull()) {
							res(v.name).Null();
						}
					}
				}
			}
		}

		for (auto _ : res("args").Array()) {
			res.String((base::path::self().remove_filename() / "lua.exe").string());

			res.String("-e");
			res.String(base::format(R"(local dbg = require [[debugger]] dbg:listen([[pipe:%s]]) dbg:start())", pipename));

			if (args.HasMember("path") && args["path"].IsString()) {
				std::string path = sourceCodingUtf8 ? args["path"].Get<std::string>() : base::u2a(args["path"]);
				res.String("-e");
				res.String(base::format("package.path=[[%s]]", path));
			}
			if (args.HasMember("cpath") && args["cpath"].IsString()) {
				std::string path = sourceCodingUtf8 ? args["cpath"].Get<std::string>() : base::u2a(args["cpath"]);
				res.String("-e");
				res.String(base::format("package.cpath=[[%s]]", path));
			}

			if (args.HasMember("arg0")) {
				if (args["arg0"].IsString()) {
					auto& v = args["arg0"];
					res.String(v);
				}
				else if (args["arg0"].IsArray()) {
					for (auto& v : args["arg0"].GetArray()) {
						if (v.IsString()) {
							res.String(v);
						}
					}
				}
			}

			std::string program = ".lua";
			if (args.HasMember("program") && args["program"].IsString()) {
				program = sourceCodingUtf8 ? args["program"].Get<std::string>() : base::u2a(args["program"]);
			}
			res.String(program);

			if (args.HasMember("arg") && args["arg"].IsArray()) {
				for (auto& v : args["arg"].GetArray()) {
					if (v.IsString()) {
						res.String(v);
					}
				}
			}
		}
	});

	vscode::io::namedpipe pipe;
	if (!pipe.open_client(pipename, 10000)) {
		return -1;
	}
	io_output(&pipe, init);
	io_output(&pipe, req);
	for (;; sleep()) {
		io.update(10);
		pipe.update(10);
		std::string buf;
		while (io.input(buf)) {
			pipe.output(buf.data(), buf.size());
		}
		while (pipe.input(buf)) {
			io.output(buf.data(), buf.size());
		}
	}
	return 0;
}
