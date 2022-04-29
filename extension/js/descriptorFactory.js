const vscode = require("vscode");
const path = require("path");
const os = require('os');
const extension = require("./extension");

function createDebugAdapterDescriptor(session, executable) {
    if (typeof session.configuration.debugServer === 'number') {
        return new vscode.DebugAdapterServer(session.configuration.debugServer);
    }
    let ROOT = extension.extensionDirectory
    let EXE = os.platform() == "win32" ? ".exe" : ""
    let runtime = path.join(ROOT, 'bin', 'lua-debug' + EXE)
    return new vscode.DebugAdapterExecutable(runtime);
}

exports.createDebugAdapterDescriptor = createDebugAdapterDescriptor;
