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
