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
    if (os.platform() == "win32") {
        if (os.arch() == "x64") {
            return process.env.VSCODE_EXTENSION_PATH + "-win32-x64"
        }
        return process.env.VSCODE_EXTENSION_PATH + "-win32-ia32"
    }
    else if (os.platform() == "darwin") {
        if (os.arch() == "arm64") {
            return process.env.VSCODE_EXTENSION_PATH + "-darwin-arm64"
        }
        return process.env.VSCODE_EXTENSION_PATH + "-darwin-x64"
    }
    else if (os.platform() == "linux") {
        if (os.arch() == "arm64") {
            return process.env.VSCODE_EXTENSION_PATH + "-linux-arm64"
        }
        return process.env.VSCODE_EXTENSION_PATH + "-linux-x64"
    }
}

async function fsExists(file) {
    return !! await fs.stat(file).catch(e => false);
}

async function copyDirectory(src, dst) {
    if (!await fsExists(src) || !await fsExists(dst)) {
        return
    }
    for (const filename of await fs.readdir(src)) {
        await fs.copyFile(path.join(src, filename), path.join(dst, filename));
    }
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
    for (const arch of ["win32-ia32", "win32-x64"]) {
        for (const luaversion of  ["lua51", "lua52", "lua53", "lua54", "lua-latest","luajit"]) {
            await copyDirectory(path.join(extensionDir, "vcredist", arch), path.join(extensionDir, "runtime", arch, luaversion))
        }
    }
    await copyDirectory(path.join(extensionDir, "vcredist", "win32-ia32"), path.join(extensionDir, "bin"))
    await fs.writeFile(installedFile, '', 'utf8');
}

async function activate(context) {
    exports.extensionDirectory = getExtensionDirectory(context)

    await install().catch(error => {
        console.log(error.stack)
    });

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
        vscode.commands.registerCommand("extension.lua-debug.runEditorContents", (uri) => {
            vscode.debug.startDebugging(vscode.workspace.getWorkspaceFolder(uri), {
                type: 'lua',
                name: 'Run Editor Contents',
                request: 'launch',
                program: uri.fsPath,
                noDebug: true
            });
        }),
        vscode.commands.registerCommand("extension.lua-debug.debugEditorContents", (uri) => {
            vscode.debug.startDebugging(vscode.workspace.getWorkspaceFolder(uri), {
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

    if (os.platform() == "win32") {
        context.subscriptions.push(
            vscode.commands.registerCommand("extension.lua-debug.pickProcess", pickProcess.pick)
        );
    }
}

exports.activate = activate;
