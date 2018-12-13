const vscode = require("vscode");
const path = require("path");
const fs = require('fs');

function getVersion(context) {
    let package = JSON.parse(fs.readFileSync(path.join(context.extensionPath, 'package.json')));
    return package.version
}

function getRuntimeDirectory(context) {
    if (path.basename(context.extensionPath) != 'extension') {
        return context.extensionPath
    }
    return path.join(process.env.USERPROFILE, '.vscode/extensions/actboy168.lua-debug-' + getVersion(context))
}

class provider {
    constructor(context) {
        this.context = context;
    }
    createDebugAdapterDescriptor(session, executable) {
        if (typeof session.configuration.debugServer === 'number') {
            return new DebugAdapterServer(session.configuration.debugServer);
        }
        let dir = getRuntimeDirectory(this.context)
        let runtime = path.join(dir, 'windows/x86/vscode-lua-debug.exe')
        let runtimeArgs = [
        ]
        return new vscode.DebugAdapterExecutable(runtime, runtimeArgs);
    }
}

exports.provider = provider;
