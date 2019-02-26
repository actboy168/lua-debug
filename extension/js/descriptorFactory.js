const vscode = require("vscode");
const path = require("path");
const fs = require('fs');
const extension = require("./extension");

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

function createDebugAdapterDescriptor(session, executable) {
    if (typeof session.configuration.debugServer === 'number') {
        return new DebugAdapterServer(session.configuration.debugServer);
    }
    let dir = getRuntimeDirectory(extension.context)
    let runtime = path.join(dir, 'client/win/lua-debug.exe')
    let runtimeArgs = [
        "-e",
        "package.path=[["+path.join(dir, 'client/?.lua')+"]]",
        path.join(dir, 'client/main.lua')
    ]
    return new vscode.DebugAdapterExecutable(runtime, runtimeArgs);
}

exports.createDebugAdapterDescriptor = createDebugAdapterDescriptor;
