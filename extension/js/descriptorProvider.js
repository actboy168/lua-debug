Object.defineProperty(exports, "__esModule", { value: true });

const vscode = require("vscode");

class provider {
    constructor(context) {
        this.context = context;
    }
    createDebugAdapterDescriptor(session, executable) {
        if (typeof session.configuration.debugServer === 'number') {
            return new DebugAdapterServer(session.configuration.debugServer);
        }
        let runtime = this.context.asAbsolutePath('./windows/x86/vscode-lua-debug.exe')
        let runtimeArgs = [
        ]
        return new vscode.DebugAdapterExecutable(runtime, runtimeArgs);
    }
}

exports.provider = provider;
