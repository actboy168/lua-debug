#include <debugger/impl.h>
#include <debugger/io/base.h>
#include <debugger/capabilities.h>
#include <debugger/luathread.h>

namespace vscode
{
	void debugger_impl::response_initialize(rprotocol& req)
	{
		if (req.HasMember("__norepl") && req["__norepl"].IsBool() && req["__norepl"].GetBool())
		{
			seq++;
			return;
		}
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("response");
			res("seq").Int64(1);
			res("command").String("initialize");
			res("request_seq").Int64(req["seq"].GetInt64());
			res("success").Bool(true);
			capabilities(res);
		}
		io_output(res);
	}

	void debugger_impl::response_thread(rprotocol& req)
	{
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("response");
			res("seq").Int64(seq++);
			res("command").String(req["command"]);
			res("request_seq").Int64(req["seq"].GetInt64());
			res("success").Bool(true);
			for (auto _ : res("body").Object())
			{
				for (auto _ : res("threads").Array())
				{
					for (auto _ : res.Object())
					{
						res("name").String("LuaThread");
						res("id").Int(1);
					}
				}
			}
		}
		io_output(res);
	}

	void debugger_impl::response_source(rprotocol& req, const char* content)
	{
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("response");
			res("seq").Int64(seq++);
			res("command").String(req["command"]);
			res("request_seq").Int64(req["seq"].GetInt64());
			res("success").Bool(true);
			for (auto _ : res("body").Object())
			{
				res("content").String(content);
				res("mimeType").String("text/x-lua");
			}
		}
		io_output(res);
	}

	void debugger_impl::event_stopped(luathread* thread, const char *msg)
	{
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("event");
			res("seq").Int64(seq++);
			res("event").String("stopped");
			for (auto _ : res("body").Object())
			{
				res("reason").String(msg);
				res("threadId").Int(thread->id);
			}
		}
		io_output(res);
	}

	void debugger_impl::event_thread(luathread* thread, bool started)
	{
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("event");
			res("seq").Int64(seq++);
			res("event").String("thread");
			for (auto _ : res("body").Object())
			{
				res("reason").String(started ? "started" : "exited");
				res("threadId").Int(thread->id);
			}
		}
		io_output(res);
	}

	void debugger_impl::event_terminated()
	{
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("event");
			res("seq").Int64(seq++);
			res("event").String("terminated");
			for (auto _ : res("body").Object())
			{
				res("restart").Bool(false);
			}
		}
		io_output(res);
	}

	void debugger_impl::event_initialized()
	{
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("event");
			res("seq").Int64(seq++);
			res("event").String("initialized");
		}
		io_output(res);
	}

	void debugger_impl::event_capabilities()
	{
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("event");
			res("seq").Int64(seq++);
			res("event").String("capabilities");
			capabilities(res);
		}
		io_output(res);
	}
	void debugger_impl::response_error(rprotocol& req, const char *msg)
	{
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("response");
			res("seq").Int64(seq++);
			res("command").String(req["command"]);
			res("request_seq").Int64(req["seq"].GetInt64());
			res("success").Bool(false);
			res("message").String(msg);
		}
		io_output(res);
	}

	void debugger_impl::response_success(rprotocol& req)
	{
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("response");
			res("seq").Int64(seq++);
			res("command").String(req["command"]);
			res("request_seq").Int64(req["seq"].GetInt64());
			res("success").Bool(true);
		}
		io_output(res);
	}

	void debugger_impl::response_success(rprotocol& req, std::function<void(wprotocol&)> body)
	{
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("response");
			res("seq").Int64(seq++);
			res("command").String(req["command"]);
			res("request_seq").Int64(req["seq"].GetInt64());
			res("success").Bool(true);
			for (auto _ : res("body").Object())
			{
				body(res);
			}
		}
		io_output(res);
	}
}
