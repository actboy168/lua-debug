#include "dbg_impl.h"
#include "dbg_io.h"
#include "dbg_capabilities.h"

namespace vscode
{
	void debugger_impl::response_initialize(rprotocol& req)
	{
		if (req.HasMember("__initseq") && req["__initseq"].IsInt())
		{
			seq = req["__initseq"].GetInt64();
		}
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
		network_->output(res);
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
		network_->output(res);
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
		network_->output(res);
	}

	void debugger_impl::event_stopped(const char *msg)
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
				res("threadId").Int(1);
			}
		}
		network_->output(res);
	}

	void debugger_impl::event_thread(bool started)
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
				res("threadId").Int(1);
			}
		}
		network_->output(res);
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
		network_->output(res);
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
		network_->output(res);
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
		network_->output(res);
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
		network_->output(res);
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
		network_->output(res);
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
		network_->output(res);
	}
}
