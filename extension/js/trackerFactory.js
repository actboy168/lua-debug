const vscode = require("vscode");

function onWillReceiveMessage(m) {
    if (m.type == "request" 
        && m.command == "setBreakpoints"
        && typeof m.arguments.source == 'object'
        && typeof m.arguments.source.path == 'string'
    ) {
        for (const doc of vscode.workspace.textDocuments) {
            if (doc.fileName == m.arguments.source.path) {
                m.arguments.sourceContent = doc.getText();
                break;
            }
        }
    }
}

function createDebugAdapterTracker(session) {
    return {
        onWillReceiveMessage : onWillReceiveMessage,
    };
}

exports.createDebugAdapterTracker = createDebugAdapterTracker;
