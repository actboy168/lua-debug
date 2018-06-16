#pragma once

#include <debugger/protocol.h>

namespace vscode
{
	inline void capabilities(wprotocol& res)
	{
		res("supportsConfigurationDoneRequest").Bool(true);
		res("supportsSetVariable").Bool(true);
		res("supportsConditionalBreakpoints").Bool(true);
		res("supportsHitConditionalBreakpoints").Bool(true);
		res("supportsDelayedStackTraceLoading").Bool(true);
		res("supportsExceptionInfoRequest").Bool(true);
		res("supportsLogPoints").Bool(true);
		res("supportsEvaluateForHovers").Bool(true);
		for (auto _ : res("exceptionBreakpointFilters").Array())
		{
			for (auto _ : res.Object())
			{
				res("default").Bool(false);
				res("filter").String("pcall");
				res("label").String("Exception: Lua pcall");
			}
			for (auto _ : res.Object())
			{
				res("default").Bool(false);
				res("filter").String("xpcall");
				res("label").String("Exception: Lua xpcall");
			}
			for (auto _ : res.Object())
			{
				res("default").Bool(true);
				res("filter").String("lua_pcall");
				res("label").String("Exception: C lua_pcall");
			}
			for (auto _ : res.Object())
			{
				res("default").Bool(true);
				res("filter").String("lua_panic");
				res("label").String("Exception: C lua_panic");
			}
		}
	}
}
