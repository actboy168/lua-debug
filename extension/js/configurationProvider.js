Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const path_1 = require("path");

class LuaConfigurationProvider {
    /**
     * Returns an initial debug configuration based on contextual information, e.g. package.json or folder.
     */
    provideDebugConfigurations(folder, token) {
        return [createLaunchConfig(folder)];
    }
    /**
     * Try to add all missing attributes to the debug configuration being launched.
     */
    resolveDebugConfiguration(folder, config, token) {
        if (!config.type && !config.request && !config.name) {
            config = createLaunchConfig(folder);
        }
        config.type = 'lua';
        config.workspaceFolder = '${workspaceFolder}';
        if (config.request != 'attach') {
            config.request = 'launch';
        }
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
                "stdout"
            ]
        }
        if (config.request == 'launch') {
            if (!config.program && !config.runtimeExecutable) {
                return vscode.window.showInformationMessage('Cannot find a program to debug').then(_ => {
                    return undefined;
                });
            }
            if (typeof config.luaexe != 'string') {
                if (typeof config.path != 'string') {
                    config.path = '${workspaceFolder}/?.lua';
                }
                if (typeof config.cpath != 'string') {
                    config.cpath = '${workspaceFolder}/?.dll';
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
}
exports.LuaConfigurationProvider = LuaConfigurationProvider;

function createLaunchConfig(folder) {
    const config = {
        type: 'lua',
        request: 'launch',
        name: 'Launch'
    };
    let program;
    const editor = vscode.window.activeTextEditor;
    if (editor && editor.document.languageId === 'lua') {
        const wf = vscode.workspace.getWorkspaceFolder(editor.document.uri);
        if (wf === folder) {
            program = vscode.workspace.asRelativePath(editor.document.uri);
            if (!path_1.isAbsolute(program)) {
                program = '${workspaceFolder}/' + program;
            }
        }
    }
    if (program) {
        config['program'] = program;
    }
    else {
        config['program'] = '${file}';
    }
    return config;
}
