const vscode = require("vscode");
const path = require("path");
const os = require('os');
const extension = require("./extension");

function createDebugAdapterDescriptor(session, executable) {
    if (typeof session.configuration.debugServer === 'number') {
        return new vscode.DebugAdapterServer(session.configuration.debugServer);
    }
    let dir = extension.extensionDirectory
    let platform = os.platform()
    if (platform == "win32") {
        let runtime = path.join(dir, 'bin/windows/lua-debug.exe')
        let runtimeArgs = [
            "-E",
            "-e",
            "package.path=[[" + path.join(dir, 'script/?.lua') + "]]",
            "-e",
            "package.cpath=[[" + path.join(dir, 'bin/windows/?.dll') + "]]",
            path.join(dir, 'script/frontend/main.lua')
        ]
        return new vscode.DebugAdapterExecutable(runtime, runtimeArgs);
    }
    else {
        let plat = platform == "darwin" ? "macos" : "linux"
        let runtime = path.join(dir, 'bin/' + plat + '/lua-debug')
        let runtimeArgs = [
            "-E",
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
