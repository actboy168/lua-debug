const vscode = require("vscode");
const path = require("path");
const os = require('os');
const fs = require('fs').promises;
const configurationProvider = require("./configurationProvider");
const descriptorFactory = require("./descriptorFactory");
const trackerFactory = require("./trackerFactory");
const pickProcess = require("./pickProcess");

function getExtensionDirectory(context) {
    const extensionPath = context.extensionPath
    if (path.basename(extensionPath) != 'extension') {
        return extensionPath
    }
    return process.env.VSCODE_EXTENSION_PATH
}

async function copyDirectory(src, dst) {
    for (const filename of await fs.readdir(src)) {
        await fs.copyFile(path.join(src, filename), path.join(dst, filename));
    }
}

async function fsExists(file) {
    return !! await fs.stat(file).catch(e => false);
}

async function install() {
    if (os.platform() != "win32") {
        return;
    }
    const extensionDir = exports.extensionDirectory;
    const installedFile = path.join(extensionDir, "vcredist", "installed");
    if (await fsExists(installedFile)) {
        return;
    }
    for (const arch of ["x86", "x86_64"]) {
        for (const luaversion of  ["lua51", "lua52", "lua53", "lua54", "lua-latest"]) {
            await copyDirectory(path.join(extensionDir, "vcredist", arch), path.join(extensionDir, "runtime", "windows", arch, luaversion))
        }
    }
    await copyDirectory(path.join(extensionDir, "vcredist", "x86"), path.join(extensionDir, "bin", "windows"))
    await fs.writeFile(installedFile, '', 'utf8');
}

async function activate(context) {
    exports.extensionDirectory = getExtensionDirectory(context)

    await install();

    for (const i in configurationProvider) {
        let provider = configurationProvider[i];
        if (provider.type == 'resolver') {
            vscode.debug.registerDebugConfigurationProvider('lua', provider);
        }
        else if (provider.type == 'provider') {
            vscode.debug.registerDebugConfigurationProvider('lua', provider, provider.triggerKind);
        }
    }

    context.subscriptions.push(
        vscode.debug.registerDebugAdapterDescriptorFactory('lua', descriptorFactory),
        vscode.debug.registerDebugAdapterTrackerFactory('lua', trackerFactory),
        vscode.commands.registerCommand("extension.lua-debug.pickProcess", pickProcess.pick),
        vscode.commands.registerCommand("extension.lua-debug.runEditorContents", (uri) => {
            vscode.debug.startDebugging(undefined, {
                type: 'lua',
                name: 'Run Editor Contents',
                request: 'launch',
                program: uri.fsPath,
                noDebug: true
            });
        }),
        vscode.commands.registerCommand("extension.lua-debug.debugEditorContents", (uri) => {
            vscode.debug.startDebugging(undefined, {
                type: 'lua',
                name: 'Debug Editor Contents',
                request: 'launch',
                program: uri.fsPath
            });
        }),
        vscode.commands.registerCommand('extension.lua-debug.showIntegerAsDec', (variable) => {
            vscode.workspace.getConfiguration("lua.debug.variables").update("showIntegerAsHex", false, vscode.ConfigurationTarget.workspace);
            const ds = vscode.debug.activeDebugSession;
            if (ds) {
                ds.customRequest('customRequestShowIntegerAsDec');
            }
        }),
        vscode.commands.registerCommand('extension.lua-debug.showIntegerAsHex', (variable) => {
            vscode.workspace.getConfiguration("lua.debug.variables").update("showIntegerAsHex", true, vscode.ConfigurationTarget.workspace);
            const ds = vscode.debug.activeDebugSession;
            if (ds) {
                ds.customRequest('customRequestShowIntegerAsHex');
            }
        })
    );
}

exports.activate = activate;
