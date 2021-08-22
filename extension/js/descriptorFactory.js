const vscode = require("vscode");
const path = require("path");
const os = require('os');
const extension = require("./extension");

function getPlatformOS() {
    let platform = os.platform()
    if (platform == "win32") {
        return "windows"
    }
    else if (platform == "darwin") {
        return "macos"
    }
    else {
        return "linux"
    }
}

function createDebugAdapterDescriptor(session, executable) {
    if (typeof session.configuration.debugServer === 'number') {
        return new vscode.DebugAdapterServer(session.configuration.debugServer);
    }
    let OS = getPlatformOS()
    let ROOT = extension.extensionDirectory
    let EXE = OS == "windows" ? ".exe" : ""
    let runtime = path.join(ROOT, 'bin', OS, 'lua-debug' + EXE)
    return new vscode.DebugAdapterExecutable(runtime);
}

exports.createDebugAdapterDescriptor = createDebugAdapterDescriptor;
