const vscode = require("vscode");
const configurationProvider = require("./configurationProvider");
const descriptorFactory = require("./descriptorFactory");
const trackerFactory = require("./trackerFactory");

function activate(context) {
    exports.context = context;
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('lua', configurationProvider));
    context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory('lua', descriptorFactory));
    context.subscriptions.push(vscode.debug.registerDebugAdapterTrackerFactory('lua', trackerFactory));
}

function deactivate() {
}

exports.activate = activate;
exports.deactivate = deactivate;
