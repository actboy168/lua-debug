const vscode = require("vscode");
const path = require("path");
const os = require('os');

const TriggerKindInitial = 1;
const TriggerKindDynamic = 2;

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

exports.initial = {
    type: "provider",
    triggerKind: TriggerKindInitial,
    provideDebugConfigurations: function (folder) {
        let program = createDefaultProgram(folder);
        if (!program) {
            program = '${file}';
        }
        return [
            {
                type: 'lua',
                request: 'launch',
                name: 'Debug',
                program: program
            }
        ];
    }
};



exports.dynamic = {
    type: "provider",
    triggerKind: TriggerKindDynamic,
    provideDebugConfigurations: function (folder) {
        let configurations = [];
        let program = createDefaultProgram(folder);
        if (program) {
            configurations.push({
                type: 'lua',
                request: 'launch',
                name: 'Debug Current File',
                program: program
            });
        }
        configurations.push({
            type: 'lua',
            request: 'attach',
            name: 'Attach TCP',
            address: "127.0.0.1:4278"
        });
        configurations.push({
            type: 'lua',
            request: 'attach',
            name: 'Attach Process',
            processId: "${command:pickProcess}"
        });
        return configurations;
    }
};

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
    delete config.windows;
    delete config.osx;
    delete config.linux;
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
    config.workspaceFolder = folder? folder.uri.fsPath: undefined;
    if (typeof config.cwd != 'string') {
        config.cwd = '${cwd}';
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
    if (typeof config.luaVersion != 'string') {
        config.luaVersion = settings.luaVersion
    }
    if (typeof config.luaArch != 'string') {
        config.luaArch = settings.luaArch
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
        if (plat == "Linux" && !config.useWSL) {
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
                        '${cwd}/?.lua',
                        path.dirname(config.luaexe) + '/?.lua',
                    ]
                }
                if (typeof config.cpath != 'string' && typeof config.cpath != 'object') {
                    if (plat == "Windows") {
                        config.cpath = [
                            '${cwd}/?.dll',
                            path.dirname(config.luaexe) + '/?.dll',
                        ]
                    }
                    else {
                        config.cpath = [
                            '${cwd}/?.so',
                            path.dirname(config.luaexe) + '/?.so',
                        ]
                    }
                }
            }
            else {
                config.sourceCoding = plat == "Windows" ? "ansi" : "utf8";
                if (typeof config.path != 'string' && typeof config.path != 'object') {
                    config.path = '${cwd}/?.lua'
                }
                if (typeof config.cpath != 'string' && typeof config.cpath != 'object') {
                    if (plat == "Windows") {
                        config.cpath = '${cwd}/?.dll'
                    }
                    else {
                        config.cpath = '${cwd}/?.so'
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
    config.configuration = {
        variables: vscode.workspace.getConfiguration("lua.debug.variables")
    }
    return config
}

exports.resolve = {
    type: "resolver",
    resolveDebugConfiguration: function (folder, config) {
        try {
            return resolveConfig(folder, config);
        } catch (err) {
            return vscode.window.showErrorMessage(err.message, { modal: true }).then(_ => undefined);
        }
    }
};
