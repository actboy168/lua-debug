Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");

class LuaConfigurationProvider {
    /**
     * Returns an initial debug configuration based on contextual information, e.g. package.json or folder.
     */
    provideDebugConfigurations(folder, token) {
    }
    /**
     * Try to add all missing attributes to the debug configuration being launched.
     */
    resolveDebugConfiguration(folder, config, token) {
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
        if (config.request == 'launch') {
            if (!config.program && !config.runtimeExecutable) {
                return vscode.window.showInformationMessage('Cannot find a program to debug').then(_ => {
                    return undefined;
                });
            }
            if (typeof config.path != 'string') {
                config.path = '${workspaceRoot}/?.lua';
            }
            if (typeof config.cpath != 'string') {
                config.cpath = '${workspaceRoot}/?.dll';
            }
        }
        else if (config.request == 'attach') {
            if (!config.ip || !config.port) {
                return vscode.window.showInformationMessage('Cannot missing ip or port to debug').then(_ => {
                    return undefined;
                });
            }
        }
        return config
    }
}
exports.LuaConfigurationProvider = LuaConfigurationProvider;
