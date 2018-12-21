const vscode = require("vscode");
const path = require("path");

function createDefaultProgram(folder) {
    let program;
    const editor = vscode.window.activeTextEditor;
    if (editor && editor.document.languageId === 'lua') {
        const wf = vscode.workspace.getWorkspaceFolder(editor.document.uri);
        if (wf === folder) {
            program = vscode.workspace.asRelativePath(editor.document.uri);
            if (!path.isAbsolute(program)) {
                program = '${workspaceFolder}/' + program;
            }
        }
    }
    if (program) {
        return program;
    }
    return '${file}';
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

function resolveDebugConfiguration(folder, config, token) {
    config.type = 'lua';
    if (config.request != 'attach') {
        config.request = 'launch';
    }
    if (typeof config.name != 'string') {
        config.name = 'Not specified';
    }
    if (typeof config.program != 'string') {
        config.program = createDefaultProgram(folder);
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
    if (typeof config.ip == 'string' && typeof config.port != 'number') {
        config.port = 4278
    }
    if (typeof config.ip != 'string' && typeof config.port == 'number') {
        config.ip = '127.0.0.1'
    }
    if (config.request == 'launch') {
        if (!config.program && !config.runtimeExecutable) {
            return vscode.window.showInformationMessage('Cannot find a program to debug').then(_ => {
                return undefined;
            });
        }
        if (typeof config.luaexe == 'string') {
            if (typeof config.path != 'string' && typeof config.path != 'object') {
                config.path = [
                    '${workspaceFolder}/?.lua',
                    path.dirname(config.luaexe) + '/?.lua',
                ]
            }
            if (typeof config.cpath != 'string' && typeof config.cpath != 'object') {
                config.cpath = [
                    '${workspaceFolder}/?.dll',
                    path.dirname(config.luaexe) + '/?.dll',
                ]
            }
        }
    }
    else if (config.request == 'attach') {
        if ((!config.ip || !config.port) && !config.processId && !config.processName) {
            return vscode.window.showInformationMessage('Cannot missing ip or port to debug').then(_ => {
                return undefined;
            });
        }
    }
    return config
}

exports.provideDebugConfigurations = provideDebugConfigurations;
exports.resolveDebugConfiguration = resolveDebugConfiguration;
