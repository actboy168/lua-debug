'use strict';

//
// 用一个假的 DAP 客户端复现：
//   目标 VM 主动加载调试器并监听端口 -> 关闭 autoUpdate（手动 update）-> 附加
//   -> 点“暂停” -> 在调试控制台执行“创建协程并立即执行”的表达式
//
// 用法: node test/repro/dap-client.js [pause|bp|step|stepbp|entry]
//   pause  : 用“暂停”进入停止状态（这个会复现问题）
//   entry  : 用 stopOnEntry 进入停止状态（同 pause，走的是同一条路径）
//   stepbp : 暂停后单步、单步撞上断点停下（另一条不经过 event.step 的停止路径）
//   bp     : 用“命中断点”进入停止状态（对照组，正常）
//   step   : 暂停后再单步（对照组，正常）
//
// 必须用 release 构建的 publish/（luamake -mode release）：
//   debug 构建里 protected_area.h 的 check_recursive() 会让停止状态下
//   重入的 rdebug.* 调用直接报 "can't recursive"，反而掩盖这个 bug。
//

const net = require('net');
const path = require('path');
const fs = require('fs');
const { spawn } = require('child_process');

const ROOT = path.resolve(__dirname, '..', '..');
const PUBLISH = path.join(ROOT, 'publish');
const LUA = (process.env.REPRO_LUA || 'lua54').trim();
const RUNTIME = path.join(PUBLISH, 'runtime', 'win32-x64', LUA);
const EXTENSION = path.join(ROOT, 'extension');
const SRC_SCRIPT = path.join(EXTENSION, 'script');
const PUB_SCRIPT = path.join(PUBLISH, 'script');
const TARGET_SCRIPT = path.join(__dirname, 'target.lua');
const TARGET_EXE = path.join(RUNTIME, 'lua.exe');
const LUADEBUG_DLL = path.join(RUNTIME, 'luadebug.dll');
const ADAPTER_EXE = path.join(PUBLISH, 'bin', 'lua-debug.exe');
const WORKER_LOG = path.join(PUBLISH, 'worker.log');

const TARGET_PORT = Number(process.env.REPRO_TARGET_PORT || 55321);
const ADAPTER_PORT = Number(process.env.REPRO_ADAPTER_PORT || 55322);
const EVAL_EXPR = 'make_coroutine()';

const mode = (process.argv[2] || 'pause').replace(/^--?mode=/, '');
if (!['pause', 'bp', 'step', 'stepbp', 'entry'].includes(mode)) {
    console.error('usage: node dap-client.js [pause|bp|step|stepbp|entry]');
    process.exit(2);
}

const sleep = ms => new Promise(r => setTimeout(r, ms));
const log = (...a) => console.log(...a);

function fail(msg) {
    console.error('FAIL: ' + msg);
    process.exit(2);
}

function checkEnv() {
    for (const f of [TARGET_EXE, LUADEBUG_DLL, ADAPTER_EXE, path.join(PUB_SCRIPT, 'debugger.lua')]) {
        if (!fs.existsSync(f)) {
            fail(`缺少 ${f}，请先 luamake -mode release 构建`);
        }
    }
}

// publish/script 是 luamake 从 extension/script 拷贝出来的（copy_extension）。
// 这里保证它是当前源码，免得测到旧脚本。
function syncScripts() {
    let copied = 0;
    const walk = (rel) => {
        for (const e of fs.readdirSync(path.join(SRC_SCRIPT, rel), { withFileTypes: true })) {
            const r = path.join(rel, e.name);
            if (e.isDirectory()) {
                fs.mkdirSync(path.join(PUB_SCRIPT, r), { recursive: true });
                walk(r);
                continue;
            }
            const src = path.join(SRC_SCRIPT, r);
            const dst = path.join(PUB_SCRIPT, r);
            const a = fs.readFileSync(src);
            if (fs.existsSync(dst) && Buffer.compare(a, fs.readFileSync(dst)) === 0) {
                continue;
            }
            fs.copyFileSync(src, dst);
            copied++;
        }
    };
    walk('');
    if (copied > 0) {
        log(`[repro] 已同步 ${copied} 个脚本到 publish/script（相当于 copy_extension）`);
    }
}

function newestMtime(dir, exts) {
    let newest = 0;
    const walk = d => {
        for (const e of fs.readdirSync(d, { withFileTypes: true })) {
            const p = path.join(d, e.name);
            if (e.isDirectory()) {
                walk(p);
            } else if (!exts || exts.some(x => e.name.endsWith(x))) {
                newest = Math.max(newest, fs.statSync(p).mtimeMs);
            }
        }
    };
    walk(dir);
    return newest;
}

function checkBinaries() {
    const dllTime = fs.statSync(LUADEBUG_DLL).mtimeMs;
    const newer = [];
    for (const d of [path.join(ROOT, 'src', 'luadebug'), path.join(ROOT, '3rd', 'lua'), path.join(ROOT, '3rd', 'bee.lua', 'binding')]) {
        const t = newestMtime(d, ['.cpp', '.c', '.h']);
        if (t > dllTime) {
            newer.push(d);
        }
    }
    if (newer.length > 0) {
        fail(`${LUADEBUG_DLL} 比这些目录里的源码旧，请先 luamake -mode release 重新构建：\n          `
            + newer.join('\n          ')
            + '\n          （debug 构建会掩盖这个问题，不能用来判断复现结果）');
    }
}

function markerLine(marker) {
    const lines = fs.readFileSync(TARGET_SCRIPT, 'utf8').split(/\r?\n/);
    for (let i = 0; i < lines.length; i++) {
        if (lines[i].includes(marker)) {
            return i + 1;
        }
    }
    throw new Error('marker not found: ' + marker);
}

function where(frame) {
    if (!frame || !frame.source) {
        return '<unknown>';
    }
    return `${frame.source.path || frame.source.name}:${frame.line}`;
}

// 停止事件本身不带位置，位置要从 stackTrace 拿（VS Code 也是这么做的）
async function locate(dap, threadId) {
    const st = await dap.request('stackTrace', { threadId, startFrame: 0, levels: 1 });
    return st.body.stackFrames[0];
}

async function connect(host, port, timeout = 10000) {
    const deadline = Date.now() + timeout;
    for (;;) {
        const socket = await new Promise(resolve => {
            const s = net.connect({ host, port });
            s.once('connect', () => resolve(s));
            s.once('error', () => { s.destroy(); resolve(null); });
        });
        if (socket) {
            socket.setNoDelay(true);
            return socket;
        }
        if (Date.now() > deadline) {
            throw new Error(`can not connect ${host}:${port}`);
        }
        await sleep(100);
    }
}

class DAP {
    constructor(socket) {
        this.socket = socket;
        this.buffer = Buffer.alloc(0);
        this.seq = 0;
        this.pending = new Map();
        this.waiters = [];
        this.events = [];
        this.stops = [];
        this.stopCursor = 0;
        socket.on('data', d => {
            this.buffer = Buffer.concat([this.buffer, d]);
            this.parse();
        });
    }

    parse() {
        for (;;) {
            const idx = this.buffer.indexOf('\r\n\r\n');
            if (idx < 0) {
                return;
            }
            const header = this.buffer.slice(0, idx).toString('latin1');
            const m = /Content-Length:\s*(\d+)/i.exec(header);
            if (!m) {
                this.buffer = this.buffer.slice(idx + 4);
                continue;
            }
            const len = Number(m[1]);
            if (this.buffer.length < idx + 4 + len) {
                return;
            }
            const body = this.buffer.slice(idx + 4, idx + 4 + len).toString('utf8');
            this.buffer = this.buffer.slice(idx + 4 + len);
            let msg;
            try {
                msg = JSON.parse(body);
            } catch (e) {
                continue;
            }
            this.dispatch(msg);
        }
    }

    dispatch(msg) {
        if (msg.type === 'response') {
            const p = this.pending.get(msg.request_seq);
            if (p) {
                this.pending.delete(msg.request_seq);
                if (msg.success) {
                    p.resolve(msg);
                } else {
                    p.reject(new Error(`${p.command} failed: ${msg.message}`));
                }
            }
            return;
        }
        if (msg.type !== 'event') {
            return;
        }
        this.events.push(msg);
        if (msg.event === 'stopped') {
            this.stops.push(msg);
        }
        if (msg.event === 'stopped' || msg.event === 'thread' || msg.event === 'initialized' || msg.event === 'breakpoint') {
            log(`  <- event ${msg.event} ${JSON.stringify(msg.body || {})}`);
        }
        for (const w of this.waiters.slice()) {
            if (w.pred(msg)) {
                clearTimeout(w.timer);
                this.waiters.splice(this.waiters.indexOf(w), 1);
                w.resolve(msg);
            }
        }
    }

    request(command, args, timeout = 10000) {
        const seq = ++this.seq;
        log(`  -> ${command}`);
        const pkg = { seq, type: 'request', command, arguments: args };
        const s = JSON.stringify(pkg);
        this.socket.write(Buffer.from(`Content-Length: ${Buffer.byteLength(s)}\r\n\r\n${s}`, 'utf8'));
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pending.delete(seq);
                reject(new Error(`timeout: ${command}`));
            }, timeout);
            this.pending.set(seq, {
                command,
                resolve: v => { clearTimeout(timer); resolve(v); },
                reject: e => { clearTimeout(timer); reject(e); },
            });
        });
    }

    waitEvent(pred, timeout = 10000, fromHistory = true) {
        if (fromHistory) {
            const found = this.events.find(pred);
            if (found) {
                return Promise.resolve(found);
            }
        }
        return new Promise((resolve, reject) => {
            const w = { pred, resolve };
            w.timer = setTimeout(() => {
                const i = this.waiters.indexOf(w);
                if (i >= 0) {
                    this.waiters.splice(i, 1);
                }
                reject(new Error('timeout: wait event'));
            }, timeout);
            this.waiters.push(w);
        });
    }

    waitStopped(timeout = 10000) {
        // 只看还没消费过的 stopped，避免拿历史事件当新事件
        if (this.stops.length > this.stopCursor) {
            return Promise.resolve(this.stops[this.stopCursor++]);
        }
        return new Promise((resolve, reject) => {
            const w = {
                pred: m => m.type === 'event' && m.event === 'stopped',
                resolve: m => { this.stopCursor++; resolve(m); },
            };
            w.timer = setTimeout(() => {
                const i = this.waiters.indexOf(w);
                if (i >= 0) {
                    this.waiters.splice(i, 1);
                }
                reject(new Error('timeout: wait stopped'));
            }, timeout);
            this.waiters.push(w);
        });
    }
}

async function main() {
    checkEnv();
    syncScripts();
    checkBinaries();
    const bpLine = markerLine('--@bp');
    const coroutineLine = markerLine('--@coroutine-first-line');
    log(`[repro] mode=${mode} bpLine=${bpLine} coroutineFirstLine=${coroutineLine}`);

    if (fs.existsSync(WORKER_LOG)) {
        fs.unlinkSync(WORKER_LOG);
    }

    const target = spawn(TARGET_EXE, [TARGET_SCRIPT, String(TARGET_PORT)], {
        cwd: RUNTIME,
        env: { ...process.env, LUA_DEBUG_ROOT: PUBLISH, LUA_DEBUG_CORE: LUADEBUG_DLL },
        stdio: ['ignore', 'pipe', 'pipe'],
    });
    target.stdout.on('data', d => log('[target] ' + d.toString().trimEnd()));
    target.stderr.on('data', d => log('[target!] ' + d.toString().trimEnd()));
    target.on('exit', (code, sig) => log(`[target] exit code=${code} signal=${sig}`));

    const adapter = spawn(ADAPTER_EXE, [String(ADAPTER_PORT)], {
        cwd: path.join(PUBLISH, 'bin'),
        stdio: ['ignore', 'pipe', 'pipe'],
    });
    adapter.stdout.on('data', d => log('[adapter] ' + d.toString().trimEnd()));
    adapter.stderr.on('data', d => log('[adapter!] ' + d.toString().trimEnd()));
    adapter.on('exit', (code, sig) => log(`[adapter] exit code=${code} signal=${sig}`));

    let ok = false;
    try {
        const socket = await connect('127.0.0.1', ADAPTER_PORT);
        const dap = new DAP(socket);

        await dap.request('initialize', {
            adapterID: 'lua',
            clientID: 'repro',
            linesStartAt1: true,
            columnsStartAt1: true,
            pathFormat: 'path',
        });

        const attach = dap.request('attach', {
            type: 'lua',
            request: 'attach',
            name: 'repro',
            address: `127.0.0.1:${TARGET_PORT}`,
            client: true, // 同 configurationProvider.js: 由调试器去 connect 目标监听的端口
            workspaceFolder: ROOT,
            luaVersion: LUA,
            stopOnEntry: mode === 'entry',
            stopOnThreadEntry: false,
        });
        const initialized = dap.waitEvent(m => m.type === 'event' && m.event === 'initialized', 15000);
        await attach;
        await initialized;
        log('[repro] attached');

        await dap.request('configurationDone', {});
        await dap.waitEvent(m => m.type === 'event' && m.event === 'thread', 10000);
        log('[repro] target thread started');

        let stop;
        if (mode === 'bp') {
            const res = await dap.request('setBreakpoints', {
                source: { path: TARGET_SCRIPT },
                breakpoints: [{ line: bpLine }],
                sourceModified: false,
            });
            log(`[repro] breakpoint: ${JSON.stringify(res.body && res.body.breakpoints)}`);
            stop = await dap.waitStopped(15000);
            log(`[repro] 断点在 ${where(await locate(dap, stop.body.threadId))} (reason=${stop.body.reason})`);
        } else if (mode === 'stepbp') {
            // 先暂停，再在停止状态下加断点，然后 stepIn 直到踩到断点行。
            // 这个停止由 event_breakpoint 直接 runLoop，不经过 event.step 的取消分支，
            // 此时 step_in 的 stepL==0 仍然挂着。
            await dap.request('pause', { threadId: 1 });
            stop = await dap.waitStopped(15000);
            log(`[repro] 暂停在 ${where(await locate(dap, stop.body.threadId))} (reason=${stop.body.reason})`);
            const res = await dap.request('setBreakpoints', {
                source: { path: TARGET_SCRIPT },
                breakpoints: [{ line: bpLine }],
                sourceModified: false,
            });
            log(`[repro] breakpoint: ${JSON.stringify(res.body && res.body.breakpoints)}`);
            for (let i = 0; i < 10 && stop.body.reason !== 'breakpoint'; i++) {
                await dap.request('stepIn', { threadId: stop.body.threadId });
                stop = await dap.waitStopped(15000);
                log(`[repro] stepIn(${i + 1}) 停在 ${where(await locate(dap, stop.body.threadId))} (reason=${stop.body.reason})`);
            }
            if (stop.body.reason !== 'breakpoint') {
                fail('stepIn 10 次都没踩到断点，无法构造出"单步被断点打断"的停止状态');
            }
        } else if (mode === 'entry') {
            stop = await dap.waitStopped(15000);
            log(`[repro] 入口停在 ${where(await locate(dap, stop.body.threadId))} (reason=${stop.body.reason})`);
        } else {
            await dap.request('pause', { threadId: 1 });
            stop = await dap.waitStopped(15000);
            log(`[repro] 暂停在 ${where(await locate(dap, stop.body.threadId))} (reason=${stop.body.reason})`);
            if (mode === 'step') {
                await dap.request('next', { threadId: stop.body.threadId });
                stop = await dap.waitStopped(15000);
                log(`[repro] 单步停在 ${where(await locate(dap, stop.body.threadId))} (reason=${stop.body.reason})`);
            }
        }

        const threadId = stop.body.threadId;
        const st = await dap.request('stackTrace', { threadId, startFrame: 0, levels: 1 });
        const frameId = st.body.stackFrames[0].id;
        log(`[repro] 停止位置 ${where(st.body.stackFrames[0])} frameId=${frameId}`);

        log(`[repro] 在调试控制台执行: ${EVAL_EXPR}`);
        const evaluate = dap
            .request('evaluate', { expression: EVAL_EXPR, frameId, context: 'repl' }, 60000)
            .then(r => ({ kind: 'response', r }), e => ({ kind: 'error', e }));

        const secondStop = await Promise.race([
            dap.waitStopped(5000).then(m => ({ kind: 'stopped', m }), () => null),
            evaluate.then(r => ({ kind: 'pending', r })),
        ]);

        if (secondStop && secondStop.kind === 'stopped') {
            // evaluate 还没返回就又收到 stopped：不管停在哪一行，都说明停止期间又被误触发了一次停止
            const frames = await dap.request('stackTrace', { threadId, startFrame: 0, levels: 3 });
            log('[repro] evaluate 还没返回，调试器却又停在:');
            for (const f of frames.body.stackFrames) {
                log(`           ${where(f)}  ${f.name}`);
            }
            log('[repro] 发送 continue，看 evaluate 是否马上返回...');
            await dap.request('continue', { threadId, allThreadsContinued: true });
            const r = await evaluate;
            log(`[repro] continue 之后 evaluate ${r.kind === 'response' ? '返回: ' + JSON.stringify(r.r.body.result) : '失败: ' + r.e}`);
            const stopLine = (frames.body.stackFrames[0] || {}).line;
            log('');
            log('===== 结论 =====');
            log('BUG 已复现：停止状态下执行“创建协程并立即执行”，evaluate 返回前又收到一次 stopped，调试控制台的表达式被阻塞');
            log(`（第二次 stopped 在 target.lua:${stopLine}，协程第一行是 target.lua:${coroutineLine}，仅作诊断）`);
        } else {
            const r = await evaluate;
            log('');
            log('===== 结论 =====');
            if (r.kind === 'response') {
                log(`BUG 未复现：evaluate 正常返回 ${JSON.stringify(r.r.body.result)}（没有多余的停止）`);
                ok = true;
            } else {
                log(`意外：evaluate 失败 ${r.e}`);
            }
        }
    } finally {
        target.kill();
        adapter.kill();
        await sleep(200);
    }

    if (fs.existsSync(WORKER_LOG)) {
        log('--- publish/worker.log ---');
        log(fs.readFileSync(WORKER_LOG, 'utf8'));
    }
    process.exit(ok ? 0 : 1);
}

main().catch(e => {
    console.error('ERROR: ' + (e && e.stack || e));
    process.exit(2);
});
