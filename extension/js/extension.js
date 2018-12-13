Object.defineProperty(exports, "__esModule", { value: true });

const vscode = require("vscode");
const configuration = require("./configurationProvider");
const descriptor = require("./descriptorProvider");

function activate(context) {
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('lua', new configuration.provider(context)));
    context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory('lua', new descriptor.provider(context)));
}

function deactivate() {
}

exports.activate = activate;
exports.deactivate = deactivate;
