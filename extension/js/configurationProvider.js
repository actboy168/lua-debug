const vscode = require("vscode");
const path = require("path");
const os = require('os');
const fs = require('fs');

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
    let files = {}
    for (const filename of fs.readdirSync(folder.uri.fsPath)) {
        if (path.extname(filename) == ".lua") {
            files[filename] = true;
        }
    }
    let keys = Object.keys(files);
    if (keys.length == 0) {
        return;
    }
    if ("main.lua" in files) {
        return '${workspaceFolder}/main.lua';
    }
    if ("test.lua" in files) {
        return '${workspaceFolder}/test.lua';
    }
    return '${workspaceFolder}/' + keys[0];
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
        if (!config.workspaceFolder) {
            throw new Error('The `cwd` can not be resolved in a multi folder workspace.\n\tSolution: "cwd" : "${workspaceFolder:name}" ');
        }
        config.cwd = config.workspaceFolder;
    }
    if (typeof config.stopOnEntry != 'boolean') {
        config.stopOnEntry = true;
    }
    if (typeof config.stopOnThreadEntry != 'boolean') {
        config.stopOnThreadEntry = false;
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
    if (typeof config.sourceCoding != 'string') {
        config.sourceCoding = settings.sourceCoding
    }
    if (typeof config.outputCapture != 'object') {
        if (config.console == "internalConsole") {
            config.outputCapture = [
                "print",
                "io.write",
                "stdout",
                "stderr"
            ]
        }
        else {
            config.outputCapture = []
        }
    }
    if (typeof config.pathFormat != 'string') {
        if (plat == "Linux" && !config.useWSL) {
            config.pathFormat = "linuxpath"
        }
        else {
            config.pathFormat = "path"
        }
    }
    if (typeof config.address == 'string' && typeof config.client != 'boolean') {
        config.client = true;
    }
    if (config.request == 'launch') {
        if (typeof config.runtimeExecutable == 'string') {
            if (config.inject !== 'hook' && typeof config.address != 'string') {
                config.console = "internalConsole";
            }
            if (typeof config.inject != 'string') {
                if (config.console == "internalConsole") {
                    if (typeof config.address == 'string') {
                        config.inject = "none";
                    }
                    else if (plat == "Windows") {
                        config.inject = "hook";
                    }
                    else {
                        config.inject = "none";
                    }
                }
                else {
                    config.inject = "none";
                }
            }
        }
        else {
            if (typeof config.program != 'string') {
                throw new Error('Missing `program` to debug');
            }
            if (typeof config.luaexe != 'string') {
                config.sourceCoding = plat == "Windows" ? "ansi" : "utf8";
            }
            // path
            if (typeof config.path == 'string') {
                config.path = [config.path]
            }
            else if (typeof config.path == 'object') {
            }
            else {
                config.path = ['${cwd}/?.lua']
                if (typeof config.luaexe == 'string') {
                    config.path.push(path.dirname(config.luaexe) + '/?.lua')
                }
            }
            if (config.path) {
                config.path = config.path.concat(settings.path)
            }
            // cpath
            if (typeof config.cpath == 'string') {
                config.cpath = [config.cpath]
            }
            else if (typeof config.cpath == 'object') {
            }
            else {
                let ext = plat == "Windows"? "dll" : "so"
                config.cpath = ['${cwd}/?.' + ext]
                if (typeof config.luaexe == 'string') {
                    config.cpath.push(path.dirname(config.luaexe) + '/?.' + ext)
                }
            }
            if (config.cpath) {
                config.cpath = config.cpath.concat(settings.cpath)
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
    resolveDebugConfiguration: async function (folder, config) {
        try {
            return resolveConfig(folder, config);
        } catch (err) { 
            let action = await vscode.window.showErrorMessage(
                err.message,
                { modal: true },
                { title: "Open launch.json" },
            );
            if (action) {
                await vscode.commands.executeCommand('workbench.action.debug.configure');
            }
            return;
        }
    }
};
