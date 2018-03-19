#pragma once

#include "dbg_protocol.h"

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
			for (auto _ : res("exceptionBreakpointFilters").Array())
			{
				for (auto _ : res.Object())
				{
					res("default").Bool(true);
					res("filter").String("error");
					res("label").String("Uncaught Error");
				}
			}
		}
	}
}
