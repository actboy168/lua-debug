#pragma once

#include <debugger/protocol.h>

namespace vscode
{
	inline void capabilities(wprotocol& res)
	{
		for (auto _ : res("body").Object())
		{
			res("supportsConfigurationDoneRequest").Bool(true);
			res("supportsSetVariable").Bool(true);
			res("supportsConditionalBreakpoints").Bool(true);
			res("supportsHitConditionalBreakpoints").Bool(true);
			res("supportsDelayedStackTraceLoading").Bool(true);
			res("supportsExceptionInfoRequest").Bool(true);
			res("supportsLogPoints").Bool(true);
			for (auto _ : res("exceptionBreakpointFilters").Array())
			{
				for (auto _ : res.Object())
				{
					res("default").Bool(true);
					res("filter").String("all");
					res("label").String("All Exceptions");
				}
				for (auto _ : res.Object())
				{
					res("default").Bool(true);
					res("filter").String("uncaught");
					res("label").String("Uncaught Exceptions");
				}
			}
		}
	}
}
