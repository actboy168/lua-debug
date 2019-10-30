const vscode = require("vscode");
const path = require("path");
const os = require('os');
const extension = require("./extension");

function getRuntimeDirectory() {
    const extensionPath = extension.context.extensionPath
    if (path.basename(extensionPath) != 'extension') {
        return extensionPath
    }
    return process.env.VSCODE_EXTENSION_PATH
}

function createDebugAdapterDescriptor(session, executable) {
    if (typeof session.configuration.debugServer === 'number') {
        return new DebugAdapterServer(session.configuration.debugServer);
    }
    let dir = getRuntimeDirectory()
    let platform = os.platform()
    if (platform == "win32") {
        let runtime = path.join(dir, 'bin/win/lua-debug.exe')
        let runtimeArgs = [
            "-e",
            "package.path=[[" + path.join(dir, 'script/?.lua') + "]]",
            path.join(dir, 'script/frontend/main.lua')
        ]
        return new vscode.DebugAdapterExecutable(runtime, runtimeArgs);
    }
    else {
        let plat = platform == "darwin" ? "macos" : "linux"
        let runtime = path.join(dir, 'bin/' + plat + '/lua-debug')
        let runtimeArgs = [
            "-e",
            "package.path=[[" + path.join(dir, 'script/?.lua') + "]]",
            "-e",
            "package.cpath=[[" + path.join(dir, 'bin/' + plat + '/?.so') + "]]",
            path.join(dir, 'script/frontend/main.lua')
        ]
        return new vscode.DebugAdapterExecutable(runtime, runtimeArgs);
    }
}

exports.createDebugAdapterDescriptor = createDebugAdapterDescriptor;
