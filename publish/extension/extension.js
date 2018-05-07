Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const configurationProvider_1 = require("./configurationProvider");
function activate(context) {
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('lua', new configurationProvider_1.LuaConfigurationProvider()));
}
exports.activate = activate;
function deactivate() {
}
exports.deactivate = deactivate;
