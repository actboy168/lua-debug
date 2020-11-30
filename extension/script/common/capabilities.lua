return {
    supportsConfigurationDoneRequest = true,
    supportsFunctionBreakpoints = true,
    supportsConditionalBreakpoints = true,
    supportsHitConditionalBreakpoints = true,
    supportsEvaluateForHovers = true,
    supportsSetVariable = true,
    supportsRestartFrame = true,
    supportsRestartRequest = true,
    supportsExceptionInfoRequest = true,
    supportsDelayedStackTraceLoading = true,
    supportsLoadedSourcesRequest = true,
    supportsLogPoints = true,
    supportsTerminateRequest = true,
    supportsClipboardContext = true,
    exceptionBreakpointFilters = {
        {
            default = false,
            filter = 'pcall',
            label = 'Exception: pcall',
        },
        {
            default = false,
            filter = 'xpcall',
            label = 'Exception: xpcall',
        },
        {
            default = true,
            filter = 'lua_pcall',
            label = 'Exception: lua_pcall',
        },
        {
            default = true,
            filter = 'lua_panic',
            label = 'Exception: lua_panic',
        }
    }
}
