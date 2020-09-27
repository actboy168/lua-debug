const vscode = require('vscode');
const os = require('os');
const child_process = require("child_process");

function exec(command) {
    return new Promise((resolve, reject) => {
        child_process.exec(command, { maxBuffer: 500 * 1024 }, (error, stdout, stderr) => {
            if (error) {
                reject(error);
                return;
            }
            if (stderr && stderr.length > 0) {
                reject(new Error(stderr));
                return;
            }
            resolve(stdout);
        });
    });
}

async function getlist(name) {
    const processes = await exec("wmic process get commandline,name,processid /FORMAT:list");
    const lines = processes.split(os.EOL);
    const processEntries = [];
    let process = {};
    for (let line of lines) {
        const res = line.match(/^(.*)=\s*(.*)\s*$/);
        if (res) {
            process[res[1]] = res[2];
        }
        else {
            if (process.Name && (name === undefined || name === process.Name)) {
                processEntries.push({
                    label: process.Name,
                    description: process.ProcessId,
                    detail: process.CommandLine,
                });
                process = {};
            }
        }
    }
    processEntries.sort((a, b) => {
        const aLower = a.label.toLowerCase();
        const bLower = b.label.toLowerCase();
        if (aLower === bLower) {
            return 0;
        }
        return aLower < bLower ? -1 : 1;
    });
    return processEntries;
}

async function pick() {
    if (os.platform() != "win32") {
        return;
    }
    const process = await vscode.window.showQuickPick(getlist(), { placeHolder: "Select process to attach to" });
    if (process) {
        return process.description;
    }
}

exports.pick = pick;
