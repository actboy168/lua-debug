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

async function WimcProcess(name, processEntries){
	const processes = await exec("wmic process get commandline,name,processid /FORMAT:list");
    const lines = processes.split(os.EOL);
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
	return process;
}

async function psProcess(name, processEntries){
	const secondColumnCharacters = 50
	const commColumnTitle = Array(secondColumnCharacters).join('a');
	let processer_shell = ''
	switch (os.platform()){
		case "darwin":
			processer_shell = `ps axww -o pid=,comm=${commColumnTitle},args= -c`;
			break
		case "linux":
			processer_shell = `ps axww -o pid=,comm=${commColumnTitle},args=`;
			break
	}
	const processes = await exec(processer_shell);
    const lines = processes.split(os.EOL);
	const parseLineFromPs = (line)=>{
		const psEntry = new RegExp(`^\\s*([0-9]+)\\s+(.{${secondColumnCharacters - 1}})\\s+(.*)$`);
        const matches = psEntry.exec(line);
        if (matches && matches.length === 4) {
            const pid = matches[1].trim();
            const executable = matches[2].trim();
            const cmdline = matches[3].trim();
            return {
				label: executable,
				description: pid,
				detail: cmdline,
			}
        }
	}
	for (let i = 1; i < lines.length; i++) {
		const line = lines[i];
		if (!line) {
			continue;
		}

		const processEntry = parseLineFromPs(line);
		if (processEntry) {
			processEntries.push(processEntry);
		}
	}

}

async function getlist(name) {
    const processEntries = [];
	switch (os.platform()){
		case "win32":
			await WimcProcess(name, processEntries)
			break
		default:
			await psProcess(name, processEntries)
			break;
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
    const process = await vscode.window.showQuickPick(getlist(), { placeHolder: "Select process to attach to" });
    if (process) {
        return process.description;
    }
}

exports.pick = pick;
