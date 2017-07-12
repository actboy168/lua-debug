#pragma once

#include "dbg_protocol.h"

namespace vscode
{
	inline void capabilities(wprotocol& res, int64_t seq)
	{
		for (auto _ : res.Object())
		{
			res("type").String("response");
			res("seq").Int64(1);
			res("command").String("initialize");
			res("request_seq").Int64(seq);
			res("success").Bool(true);
			for (auto _ : res("body").Object())
			{
				res("supportsConfigurationDoneRequest").Bool(true);
				res("supportsSetVariable").Bool(true);
				res("supportsConditionalBreakpoints").Bool(true);
				res("supportsDelayedStackTraceLoading").Bool(true);
				for (auto _ : res("exceptionBreakpointFilters").Array())
				{
					for (auto _ : res.Object())
					{
						res("filter").String("error");
						res("label").String("Uncaught Error");
					}
				}
			}
		}
	}
}
