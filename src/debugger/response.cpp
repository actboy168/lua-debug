#include <debugger/impl.h>
#include <debugger/io/base.h>
#include <debugger/capabilities.h>
#include <debugger/luathread.h>
#include <base/util/format.h>

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
            for (auto _ : res("body").Object())
            {
                capabilities(res);
            }
		}
		io_output(res);
	}

	void debugger_impl::response_threads(rprotocol& req)
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
					for (auto& lt : luathreads_) 
					{
						for (auto _ : res.Object())
						{
							res("name").String(base::format("Thread %d", lt.second->id));
							res("id").Int(lt.second->id);
						}
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

	void debugger_impl::event_stopped(luathread* thread, const char *msg, const char* description)
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
				if (description) {
					res("text").String(description);
				}
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
			for (auto _ : res("body").Object())
			{
				for (auto _ : res("capabilities").Object())
				{
					capabilities(res);
				}
			}
		}
		io_output(res);
	}

	void debugger_impl::event_breakpoint(const char* reason, bp_breakpoint* bp)
	{
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("event");
			res("seq").Int64(seq++);
			res("event").String("breakpoint");
			for (auto _ : res("body").Object())
			{
				res("reason").String(reason);
				for (auto _ : res("breakpoint").Object())
				{
					// new
					//if (reason[0] == 'n') {
					//	src->output(res);
					//}
					bp->output(res);
				}
			}
		}
		io_output(res);
	}

	void debugger_impl::event_loadedsource(const char* reason, source* src)
	{
		wprotocol res;
		for (auto _ : res.Object())
		{
			res("type").String("event");
			res("seq").Int64(seq++);
			res("event").String("loadedSource");
			for (auto _ : res("body").Object())
			{
				res("reason").String(reason);
				src->output(res);
			}
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
