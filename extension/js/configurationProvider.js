const vscode = require("vscode");
const path = require("path");
const os = require('os');

function createDefaultProgram(folder) {
    const editor = vscode.window.activeTextEditor;
    if (editor && editor.document.languageId === 'lua') {
        const wf = vscode.workspace.getWorkspaceFolder(editor.document.uri);
        if (wf === folder) {
            let program = vscode.workspace.asRelativePath(editor.document.uri);
            if (!path.isAbsolute(program)) {
                program = '${workspaceFolder}/' + program;
            }
            return program;
        }
    }
}

function provideDebugConfigurations(folder, token) {
    let program = createDefaultProgram(folder);
    if (!program) {
        program = '${file}';
    }
    return [
        {
            type: 'lua',
            request: 'launch',
            name: 'Launch',
            program: program
        }
    ];
}

function mergeConfigurations(config) {
    let platname
    let platcfg
    if (os.platform() == "win32" && !config.useWSL) {
        platname = "Windows"
        platcfg = config.windows
    }
    else if (os.platform() == "darwin") {
        platname = "macOS"
        platcfg = config.osx
    }
    else {
        platname = "Linux"
        platcfg = config.linux
    }
    config.windows = null
    config.osx = null
    config.linux = null
    if (typeof platcfg == 'object') {
        for (var k in platcfg) {
            config[k] = platcfg[k]
        }
    }
    return platname
}

function resolveConfig(folder, config) {
    const settings = vscode.workspace.getConfiguration("lua.debug.settings");
    const plat = mergeConfigurations(config)
    config.type = 'lua';
    if (config.request != 'attach') {
        config.request = 'launch';
    }
    if (typeof config.name != 'string') {
        config.name = 'Not specified';
    }
    config.workspaceFolder = '${workspaceFolder}';
    if (typeof config.cwd != 'string') {
        config.cwd = '${workspaceFolder}';
    }
    if (typeof config.stopOnEntry != 'boolean') {
        config.stopOnEntry = true;
    }
    if (typeof config.stopOnThreadEntry != 'boolean') {
        config.stopOnThreadEntry = false;
    }
    if (typeof config.termOnExit != 'boolean') {
        config.termOnExit = config.request == 'launch';
    }
    if (typeof config.luaRuntime != 'string') {
        config.luaRuntime = settings.luaRuntime
    }
    if (typeof config.console != 'string') {
        config.console = settings.console
    }
    if (typeof config.consoleCoding != 'string') {
        config.consoleCoding = settings.consoleCoding
    }
    if (typeof config.sourceCoding != 'string') {
        config.sourceCoding = settings.sourceCoding
    }
    if (typeof config.outputCapture != 'object') {
        config.outputCapture = [
            "print",
            "io.write",
            "stderr"
        ]
    }
    if (typeof config.pathFormat != 'string') {
        if (plat == "Linux") {
            config.pathFormat = "linuxpath"
        }
        else {
            config.pathFormat = "path"
        }
    }
    if (typeof config.address != 'string') {
        config.address = "127.0.0.1:4278"
    }
    if (typeof config.client != 'boolean') {
        config.client = true
    }
    if (config.request == 'launch') {
        if (typeof config.runtimeExecutable != 'string') {
            if (typeof config.program != 'string') {
                config.program = createDefaultProgram(folder);
                if (typeof config.program != 'string') {
                    throw new Error('Missing `program` to debug');
                }
            }
            if (typeof config.luaexe == 'string') {
                if (typeof config.path != 'string' && typeof config.path != 'object') {
                    config.path = [
                        '${workspaceFolder}/?.lua',
                        path.dirname(config.luaexe) + '/?.lua',
                    ]
                }
                if (typeof config.cpath != 'string' && typeof config.cpath != 'object') {
                    if (plat == "Windows") {
                        config.cpath = [
                            '${workspaceFolder}/?.dll',
                            path.dirname(config.luaexe) + '/?.dll',
                        ]
                    }
                    else {
                        config.cpath = [
                            '${workspaceFolder}/?.so',
                            path.dirname(config.luaexe) + '/?.so',
                        ]
                    }
                }
            }
            else {
                if (typeof config.path != 'string' && typeof config.path != 'object') {
                    config.path = '${workspaceFolder}/?.lua'
                }
                if (typeof config.cpath != 'string' && typeof config.cpath != 'object') {
                    if (plat == "Windows") {
                        config.cpath = '${workspaceFolder}/?.dll'
                    }
                    else {
                        config.cpath = '${workspaceFolder}/?.so'
                    }
                }
            }
        }
    }
    else if (config.request == 'attach') {
        if (!config.address && !config.processId && !config.processName) {
            throw new Error('Missing `address` to debug');
        }
    }
    return config
}

function resolveDebugConfiguration(folder, config, token) {
    try {
        return resolveConfig(folder, config);
    } catch (err) {
        return vscode.window.showErrorMessage(err.message, { modal: true }).then(_ => undefined);
    }
}

exports.provideDebugConfigurations = provideDebugConfigurations;
exports.resolveDebugConfiguration = resolveDebugConfiguration;
