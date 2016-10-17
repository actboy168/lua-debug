#include "dbg_impl.h"
#include "dbg_network.h"
#include "dbg_message.h"

namespace vscode
{
	void debugger_impl::response_initialize(rprotocol& req)
	{
		if (norepl_initialize_)
		{
			seq++;
			return;
		}
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
				res("supportsConfigurationDoneRequest").Bool(true);
				res("supportsSetVariable").Bool(true);
				res("supportsEvaluateForHovers").Bool(false);
				res("supportsFunctionBreakpoints").Bool(false);
				res("supportsConditionalBreakpoints").Bool(true);
				res("exceptionBreakpointFilters").Bool(false);
				res("supportsStepBack").Bool(false);
				res("supportsRestartFrame").Bool(false);
				res("supportsGotoTargetsRequest").Bool(false);
				res("supportsStepInTargetsRequest").Bool(false);
				res("supportsCompletionsRequest").Bool(false);
				
			}
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
			}
		}
		network_->output(res);
	}

	void debugger_impl::event_stopped(const char *msg)
	{
		watch_.clear();

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
