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
    let plat
    if (os.platform() == "win32") {
        plat = config.windows
    }
    else if (os.platform() == "darwin") {
        plat = config.osx
    }
    else {
        plat = config.linux
    }
    config.windows = null
    config.osx = null
    config.linux = null
    if (typeof plat != 'object') {
        return
    }
    for (var k in plat) {
        config[k] = plat[k]
    }
}

function resolveDebugConfiguration(folder, config, token) {
    mergeConfigurations(config)
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
        config.sourceCoding = 'ansi'
    }
    if (typeof config.outputCapture != 'object') {
        config.outputCapture = [
            "print",
            "io.write",
            "stderr"
        ]
    }
    if (typeof config.pathFormat != 'string') {
        if (os.platform() == "win32" || os.platform() == "darwin") {
            config.pathFormat = "path"
        }
        else {
            config.pathFormat = "linuxpath"
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
                    if (os.platform() == "win32") {
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
                    if (os.platform() == "win32") {
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
