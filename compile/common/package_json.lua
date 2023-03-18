local platform = ...
platform = platform or "unknown-unknown"

local OS, ARCH = platform:match "^([^-]+)-([^-]+)$"

local json = {
    name = "lua-debug",
    version = "1.61.0",
    publisher = "actboy168",
    displayName = "Lua Debug",
    description = "VSCode debugger extension for Lua",
    icon = "images/logo.png",
    private = true,
    author = {
        name = "actboy168",
    },
    bugs = {
        url = "https://github.com/actboy168/lua-debug/issues",
    },
    repository = {
        type = "git",
        url = "https://github.com/actboy168/lua-debug",
    },
    keywords = {
        "lua",
        "debug",
        "debuggers",
    },
    categories = {
        "Debuggers",
    },
    engines = {
        vscode = "^1.61.0",
    },
    extensionKind = {
        "workspace",
    },
    main = "./js/extension.js",
    activationEvents = {
        "onCommand:extension.lua-debug.runEditorContents",
        "onCommand:extension.lua-debug.debugEditorContents",
        "onDebugInitialConfigurations",
        "onDebugDynamicConfigurations",
        "onDebugResolve:lua",
    },
    capabilities = {
        untrustedWorkspaces = {
            description = "Debugging is disabled in Restricted Mode.",
            supported = false,
        },
    },
    contributes = {
        breakpoints = {
            {
                language = "lua",
            },
            {
                language = "html",
            },
        },
        commands = {
            {
                command = "extension.lua-debug.runEditorContents",
                icon = "$(play)",
                title = "Run File",
            },
            {
                command = "extension.lua-debug.debugEditorContents",
                icon = "$(debug-alt-small)",
                title = "Debug File",
            },
            {
                command = "extension.lua-debug.showIntegerAsDec",
                title = "Show as Dec",
            },
            {
                command = "extension.lua-debug.showIntegerAsHex",
                title = "Show as Hex",
            },
        },
        configuration = {
            properties = {
                ["lua.debug.variables.showIntegerAsHex"] = {
                    default = false,
                    description = "Show integer as hex.",
                    type = "boolean",
                },
            },
        },
        debuggers = {
            {
                type = "lua",
                languages = {
                    "lua",
                },
                label = "Lua Debug",
                configurationSnippets = {
                    {
                        label = "Lua Debug: Launch Script",
                        description = "A new configuration for launching a lua debug program",
                        body = {
                            type = "lua",
                            request = "launch",
                            name = "${1:launch}",
                            stopOnEntry = true,
                            program = "^\"\\${workspaceFolder}/${2:main.lua}\"",
                            arg = {
                            },
                        },
                    },
                    {
                        label = "Lua Debug: Attach",
                        description = "A new configuration for attaching a lua debug program",
                        body = {
                            type = "lua",
                            request = "attach",
                            name = "${1:attach}",
                            stopOnEntry = true,
                            address = "127.0.0.1:4278",
                        }
                    }
                }
            }
        },
        menus = {
            ["debug/variables/context"] = {
                {
                    command = "extension.lua-debug.showIntegerAsDec",
                    group = "1_view",
                    when = "debugConfigurationType == 'lua' && debugProtocolVariableMenuContext == 'integer/hex'",
                },
                {
                    command = "extension.lua-debug.showIntegerAsHex",
                    group = "1_view",
                    when = "debugConfigurationType == 'lua' && debugProtocolVariableMenuContext == 'integer/dec'",
                },
            },
            ["editor/title/run"] = {
                {
                    command = "extension.lua-debug.runEditorContents",
                    when = "resourceLangId == lua",
                },
                {
                    command = "extension.lua-debug.debugEditorContents",
                    when = "resourceLangId == lua",
                },
            },
        },
    },
}

local attributes = {}

attributes.common = {
    luaVersion = {
        default = "5.4",
        enum = {
            "5.1",
            "5.2",
            "5.3",
            "5.4",
            "latest",
            "jit",
        },
        markdownDescription = "%lua.debug.launch.luaVersion.description%",
        type = "string",
    },
    outputCapture = {
        default = {
        },
        items = {
            enum = {
                "print",
                "io.write",
                "stdout",
                "stderr",
            },
        },
        markdownDescription = "From where to capture output messages: print or stdout/stderr streams.",
        type = "array",
    },
    pathFormat = {
        default = "path",
        enum = {
            "path",
            "linuxpath",
        },
        markdownDescription = "Path format",
        type = "string",
    },
    sourceFormat = {
        default = "path",
        enum = {
            "path",
            "string",
            "linuxpath",
        },
        markdownDescription = "Source format",
        type = "string",
    },
    sourceMaps = {
        default = {
            {
                "./*",
                "${workspaceFolder}/*",
            },
        },
        markdownDescription = "The source path of the remote host and the source path of local.",
        type = "array",
    },
    skipFiles = {
        default = {
        },
        items = {
            type = "string",
        },
        markdownDescription = "An array of glob patterns for files to skip when debugging.",
        type = "array",
    },
    stopOnEntry = {
        default = false,
        markdownDescription = "Automatically stop after entry.",
        type = "boolean",
    },
    stopOnThreadEntry = {
        default = true,
        markdownDescription = "Automatically stop after thread entry.",
        type = "boolean",
    },
    address = {
        default = "127.0.0.1:4278",
        markdownDescription = [[
Debugger address.
1. IPv4 e.g. `127.0.0.1:4278`
2. IPv6 e.g. `[::1]:4278`
3. Unix domain socket e.g. `@c:\\unix.sock`]],
        type = {
            "string",
            "null",
        },
    },
    client = {
        default = true,
        markdownDescription = "Choose whether to `connect` or `listen`.",
        type = "boolean",
    },
    inject = {
        default = "none",
        markdownDescription = "How to inject debugger.",
        enum = {
            "none",
        },
        type = "string",
    },
}

if OS == "win32" then
    attributes.common.inject.default = "hook"
    table.insert(attributes.common.inject.enum, "hook")
elseif OS == "darwin" then
    attributes.common.inject.default = "lldb"
    table.insert(attributes.common.inject.enum, "hook")
    table.insert(attributes.common.inject.enum, "lldb")
    attributes.common.inject_executable = {
        markdownDescription = "inject executable path",
        type = {
            "string",
            "null",
        },
    }
end

attributes.attach = {
}

if OS == "win32" or OS == "darwin" then
    attributes.attach.processId = {
        default = "${command:pickProcess}",
        markdownDescription = "Id of process to attach to.",
        type = "string",
    }
    attributes.attach.processName = {
        default = "lua.exe",
        markdownDescription = "Name of process to attach to.",
        type = "string",
    }
    json.activationEvents[#json.activationEvents+1] = "onCommand:extension.lua-debug.pickProcess"
    json.contributes.debuggers[1].variables = {
        pickProcess = "extension.lua-debug.pickProcess",
    }
end

attributes.launch = {
    luaexe = {
        markdownDescription = "Absolute path to the lua exe.",
        type = "string",
    },
    program = {
        default = "${workspaceFolder}/main.lua",
        markdownDescription = "Lua program to debug - set this to the path of the script",
        type = "string",
    },
    arg = {
        default = {
        },
        markdownDescription = "Command line argument, arg[1] ... arg[n]",
        type = "array",
    },
    arg0 = {
        default = {
        },
        markdownDescription = "Command line argument, arg[-n] ... arg[0]",
        type = {
            "string",
            "array",
        },
    },
    path = {
        default = "${workspaceFolder}/?.lua",
        markdownDescription = "%lua.debug.launch.path.description%",
        type = {
            "string",
            "array",
            "null",
        },
    },
    cpath = {
        markdownDescription = "%lua.debug.launch.cpath.description%",
        type = {
            "string",
            "array",
            "null",
        },
    },
    luaArch = {
        markdownDescription = "%lua.debug.launch.luaArch.description%",
        type = "string",
    },
    cwd = {
        default = "${workspaceFolder}",
        markdownDescription = "Working directory at program startup",
        type = {
            "string",
            "null",
        },
    },
    env = {
        additionalProperties = {
            type = {
                "string",
                "null",
            },
        },
        default = {
            PATH = "${workspaceFolder}",
        },
        markdownDescription = "Environment variables passed to the program. The value `null` removes thevariable from the environment.",
        type = "object",
    },
    console = {
        default = "integratedTerminal",
        enum = {
            "internalConsole",
            "integratedTerminal",
            "externalTerminal",
        },
        enummarkdownDescriptions = {
            "%lua.debug.launch.console.internalConsole.description%",
            "%lua.debug.launch.console.integratedTerminal.description%",
            "%lua.debug.launch.console.externalTerminal.description%",
        },
        markdownDescription = "%lua.debug.launch.console.description%",
        type = "string",
    },
    runtimeExecutable = {
        default = OS == "win32" and "${workspaceFolder}/lua.exe" or "${workspaceFolder}/lua",
        markdownDescription = "Runtime to use. Either an absolute path or the name of a runtime availableon the PATH.",
        type = {
            "string",
            "null",
        },
    },
    runtimeArgs = {
        default = "${workspaceFolder}/main.lua",
        markdownDescription = "Arguments passed to the runtime executable.",
        type = {
            "string",
            "array",
            "null",
        },
    },
}

if OS == "win32" or OS == "darwin" then
    local snippets = json.contributes.debuggers[1].configurationSnippets
    snippets[#snippets+1] = {
        label = "Lua Debug: Launch Process",
        description = "A new configuration for launching a lua process",
        body = {
            type = "lua",
            request = "launch",
            name = "${1:launch process}",
            stopOnEntry = true,
            runtimeExecutable = "^\"\\${workspaceFolder}/lua.exe\"",
            runtimeArgs = "^\"\\${workspaceFolder}/${2:main.lua}\"",
        }
    }
    snippets[#snippets+1] = {
        label = "Lua Debug: Attach Process",
        description = "A new configuration for attaching a lua debug program",
        body = {
            type = "lua",
            request = "attach",
            name = "${1:attach}",
            stopOnEntry = true,
            processId = "^\"\\${command:pickProcess}\"",
        }
    }
end

if OS == "win32" then
    attributes.common.sourceCoding = {
        default = "utf8",
        enum = {
            "utf8",
            "ansi",
        },
        markdownDescription = "%lua.debug.launch.sourceCoding.description%",
        type = "string",
    }
    attributes.common.useWSL = {
        default = true,
        description = "Use Windows Subsystem for Linux.",
        type = "boolean",
    }
    attributes.launch.luaexe.default = "${workspaceFolder}/lua.exe"
    attributes.launch.cpath.default = "${workspaceFolder}/?.dll"
else
    attributes.launch.luaexe.default = "${workspaceFolder}/lua"
    attributes.launch.cpath.default = "${workspaceFolder}/?.so"
end

local function SupportedArchs()
    if OS == "win32" then
        return "x86_64", "x86"
    elseif OS == "darwin" then
        if ARCH == "arm64" then
            return "arm64", "x86_64"
        else
            return "x86_64"
        end
    elseif OS == "linux" then
        if ARCH == "arm64" then
            return "arm64"
        else
            return "x86_64"
        end
    end
end

local Archs = { SupportedArchs() }
attributes.launch.luaArch.default = Archs[1]
attributes.launch.luaArch.enum = Archs

for k, v in pairs(attributes.common) do
    attributes.attach[k] = v
    attributes.launch[k] = v
end
json.contributes.debuggers[1].configurationAttributes = {
    launch = { properties = attributes.launch },
    attach = { properties = attributes.attach },
}

local configuration = json.contributes.configuration.properties
for _, name in ipairs { "luaArch", "luaVersion", "sourceCoding", "path", "cpath", "console" } do
    local attr = attributes.launch[name] or attributes.attach[name]
    if attr then
        local cfg = {}
        for k, v in pairs(attr) do
            if k == 'markdownDescription' then
                k = 'description'
            end
            if k == 'enummarkdownDescriptions' then
                k = 'enumDescriptions'
            end
            cfg[k] = v
        end
        configuration["lua.debug.settings."..name] = cfg
    end
end

return json
