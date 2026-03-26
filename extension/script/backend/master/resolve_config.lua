--- 校验并补全调试配置
--- @param config table 原始配置表
--- @return table|nil config 校验后的配置表，失败时为 nil
--- @return string|nil errmsg 错误信息，成功时为 nil
local function resolve_config(config)
    -- 通用默认值填充
    config.type = "lua"
    if config.request ~= "attach" then
        config.request = "launch"
    end
    if type(config.name) ~= "string" then
        config.name = "Not specified"
    end
    if type(config.cwd) ~= "string" then
        if type(config.workspaceFolder) == "string" then
            config.cwd = config.workspaceFolder
        end
    end
    if type(config.stopOnEntry) ~= "boolean" then
        config.stopOnEntry = true
    end
    if type(config.stopOnThreadEntry) ~= "boolean" then
        config.stopOnThreadEntry = false
    end
    if type(config.luaVersion) ~= "string" then
        config.luaVersion = "lua54"
    end
    if type(config.console) ~= "string" then
        config.console = "internalConsole"
    end
    if type(config.sourceCoding) ~= "string" then
        config.sourceCoding = "utf8"
    end
    if type(config.outputCapture) ~= "table" then
        if config.console == "internalConsole" then
            config.outputCapture = {
                "print",
                "io.write",
                "stdout",
                "stderr",
            }
        else
            config.outputCapture = {}
        end
    end
    if type(config.pathFormat) ~= "string" then
        if config.useWSL then
            config.pathFormat = "path"
        else
            ---@NOTICE 后端无法获取前端的操作系统，只能假设和后端是一致的
            local platform = require 'bee.platform'
            if platform.os == "windows" or platform.os == "macos" then
                config.pathFormat = "path"
            else
                config.pathFormat = "linuxpath"
            end
        end
    end

    -- sourceMaps 校验
    if type(config.sourceMaps) == "table" then
        for _, sourceMap in ipairs(config.sourceMaps) do
            if type(sourceMap) ~= "table" or #sourceMap ~= 2 then
                error "Invalid sourceMaps."
            end
        end
    else
        config.sourceMaps = nil
    end

    -- client 默认值
    if type(config.address) == "string" and type(config.client) ~= "boolean" then
        config.client = true
    end

    -- configuration.variables 默认值（后端无法获取 VSCode settings，使用空表）
    if type(config.configuration) ~= "table" then
        config.configuration = {
            variables = {},
        }
    end
end

return resolve_config
