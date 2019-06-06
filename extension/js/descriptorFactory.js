const vscode = require("vscode");
const path = require("path");
const fs = require('fs');
const os = require('os');
const extension = require("./extension");

function getVersion(context) {
    let package = JSON.parse(fs.readFileSync(path.join(context.extensionPath, 'package.json')));
    return package.version
}

function getHomeDirectory() {
    if (os.platform() != 'win32') {
        return process.env.HOME
    }
    return process.env.USERPROFILE
}

function getRuntimeDirectory(VSCODE) {
    let context = extension.context
    if (path.basename(context.extensionPath) != 'extension') {
        return context.extensionPath
    }
    return path.join(getHomeDirectory(), VSCODE + '/extensions/actboy168.lua-debug-' + getVersion(context))
}

function createDebugAdapterDescriptor(session, executable) {
    if (typeof session.configuration.debugServer === 'number') {
        return new DebugAdapterServer(session.configuration.debugServer);
    }
    // TODO: .vscode-insiders
    let VSCODE = extension.context.executionContext === vscode.ExtensionExecutionContext.Remote
        ? ".vscode-server"
        : ".vscode";
    let dir = getRuntimeDirectory(VSCODE)
    let platform = os.platform()
    if (platform == "win32") {
        let runtime = path.join(dir, 'bin/win/lua-debug.exe')
        let runtimeArgs = [
            "-e",
            "package.path=[["+path.join(dir, 'script/?.lua')+"]]",
            "-e", "VSCODE='"+VSCODE+"'",
            path.join(dir, 'script/frontend/main.lua')
        ]
        return new vscode.DebugAdapterExecutable(runtime, runtimeArgs);
    }
    else {
        let plat = platform == "darwin"? "macos": "linux"
        let runtime = path.join(dir, 'bin/'+plat+'/lua-debug')
        let runtimeArgs = [
            "-e",
            "package.path=[["+path.join(dir, 'script/?.lua')+"]]",
            "-e",
            "package.cpath=[["+path.join(dir, 'bin/'+plat+'/?.so')+"]]",
            "-e", "VSCODE='"+VSCODE+"'",
            path.join(dir, 'script/frontend/main.lua')
        ]
        return new vscode.DebugAdapterExecutable(runtime, runtimeArgs);
    }
}

exports.createDebugAdapterDescriptor = createDebugAdapterDescriptor;
exports.getRuntimeDirectory = getRuntimeDirectory;
