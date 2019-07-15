const vscode = require("vscode");
const path = require("path");
const fs = require('fs');
const os = require('os');
const extension = require("./extension");

function getDataFolderName() {
    const product = JSON.parse(fs.readFileSync(path.join(vscode.env.appRoot, 'product.json')));
    if (vscode.ExtensionExecutionContext == undefined) {
        return product.dataFolderName;
    }
    if (extension.context.executionContext == vscode.ExtensionExecutionContext.Local) {
        return product.dataFolderName;
    }
    return product.serverDataFolderName;
}

function getVersion(context) {
    const package = JSON.parse(fs.readFileSync(path.join(context.extensionPath, 'package.json')));
    return package.version
}

function getHomeDirectory() {
    if (os.platform() != 'win32') {
        return process.env.HOME
    }
    return process.env.USERPROFILE
}

function getRuntimeDirectory() {
    const context = extension.context
    if (path.basename(context.extensionPath) != 'extension') {
        return context.extensionPath
    }
    return path.join(getHomeDirectory(), getDataFolderName() + '/extensions/actboy168.lua-debug-' + getVersion(context))
}

function createDebugAdapterDescriptor(session, executable) {
    if (typeof session.configuration.debugServer === 'number') {
        return new DebugAdapterServer(session.configuration.debugServer);
    }
    let VSCODE = getDataFolderName()
    let dir = getRuntimeDirectory()
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
