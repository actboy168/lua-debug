const vscode = require("vscode");
const fs = require('fs');

function onWillReceiveMessage(m) {
    if (m.type == "request" 
        && m.command == "setBreakpoints"
        && typeof m.arguments.source == 'object'
        && typeof m.arguments.source.path == 'string'
    ) {
        for (const doc of vscode.workspace.textDocuments) {
            if (doc.fileName == m.arguments.source.path) {
                m.arguments.sourceContent = doc.getText();
                return;
            }
        }
        // TODO: use vscode api?
        try {
            m.arguments.sourceContent = fs.readFileSync(m.arguments.source.path, 'utf8')
        } catch (error) {
        }
    }
}

function createDebugAdapterTracker(session) {
    return {
        onWillReceiveMessage : onWillReceiveMessage,
    };
}

exports.createDebugAdapterTracker = createDebugAdapterTracker;
