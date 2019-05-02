const vscode = require("vscode");
const path = require("path");
const os = require('os');
const fs = require('fs');
const descriptorFactory = require("./descriptorFactory");

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
    return [
        {
            type: 'lua',
            request: 'launch',
            name: 'Launch',
            program: createDefaultProgram(folder)
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

function checkRuntime() {
    let platform = os.platform()
    if (platform == "win32") {
        return true
    }
    let plat = platform == "darwin"? "macos": "linux"
    let dir = descriptorFactory.getRuntimeDirectory()
    let runtime = path.join(dir, 'bin/'+plat.toLowerCase()+'/lua-debug')
    return fs.existsSync(runtime)
}

function resolveDebugConfiguration(folder, config, token) {
    if (!checkRuntime()) {
        return vscode.window.showErrorMessage('Non-Windows need to compile lua-debug first.').then(_ => {
            return undefined;
        });
    }
    let plat = mergeConfigurations(config)
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
    if (typeof config.consoleCoding != 'string') {
        config.consoleCoding = 'utf8'
    }
    if (typeof config.sourceCoding != 'string') {
        if (plat == "Windows") {
            config.sourceCoding = 'ansi'
        }
        else {
            config.sourceCoding = 'utf8'
        }
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
    if (typeof config.ip == 'string' && typeof config.port != 'number') {
        config.port = 4278
    }
    if (typeof config.ip != 'string' && typeof config.port == 'number') {
        config.ip = '127.0.0.1'
    }
    if (config.request == 'launch') {
        if (typeof config.runtimeExecutable != 'string') {
            if (typeof config.program != 'string') {
                config.program = createDefaultProgram(folder);
                if (typeof config.program != 'string') {
                    return vscode.window.showErrorMessage('Cannot find a program to debug').then(_ => {
                        return undefined;
                    });
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
        if ((!config.ip || !config.port) && !config.processId && !config.processName) {
            return vscode.window.showErrorMessage('Cannot missing ip or port to debug').then(_ => {
                return undefined;
            });
        }
    }
    return config
}

exports.provideDebugConfigurations = provideDebugConfigurations;
exports.resolveDebugConfiguration = resolveDebugConfiguration;
