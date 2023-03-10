local fs = require 'bee.filesystem'
local sp = require 'bee.subprocess'
local arch = require "bee.platform".Arch
local platform_os = require 'frontend.platform_os' ()

local _M = {}

local macos = "macOS"
local windows = "Windows"
local entry_launch = "launch"

local function macos_check_rosetta_process(process)
    local rosetta_runtime <const> = "/usr/libexec/rosetta/runtime"
    if not fs.exists(rosetta_runtime) then
        return false
    end
    local p = sp.spawn {
        "/usr/bin/fuser",
        rosetta_runtime,
        stdout = true,
        stderr = true, -- for skip  fuser output
    }
    if not p then
        return false
    end
    local l = p.stdout:read "a"
    return l:find(tostring(process)) ~= nil
end

function _M.lldb_inject(pid, entry, injectdll, lldb_path)
    injectdll = injectdll or (WORKDIR / "bin" / "launcher.so"):string()
    lldb_path = lldb_path or "lldb"
    local pre_luancher = entry == entry_launch and
        {
            "-o",
            "breakpoint set -n main",
            "-o",
            "c",
        } or {}

    local launcher = {
        "-o",
        -- 6 = RTDL_NOW|RTDL_LOCAL
        ('expression (void*)dlopen("%s", 6)'):format(injectdll),
        "-o",
        ('expression ((void(*)())&%s)()'):format(entry),
        "-o",
        "quit"
    }

    local p, err = sp.spawn {
        lldb_path,
        "-p", tostring(pid),
        "--batch",
        pre_luancher,
        launcher,
        stdout = true,
        stderr = true,
    }
    if not p then
        return false, "Spwan lldb failed:" .. err
    end
    if p:wait() ~= 0 then
        return false, "stdout:" .. p.stdout:read "a" .. "\nstderr:" .. p.stderr:read "a"
    end
    return true
end

function _M.macos_inject(process, entry, injectdll)
    injectdll = injectdll or (WORKDIR / "bin" / "launcher.so"):string()
    local helper = (WORKDIR / "bin" / "process_inject_helper"):string()
    if not fs.exists(injectdll) then
        return false, "Not found launcher.so."
    end
    local p, err = sp.spawn {
        "/usr/bin/osascript",
        "-e",
        ([[do shell script "%s %d %s %s" with administrator privileges with prompt "lua-debug"]]):format(helper, process, injectdll, entry),
        stderr = true,
    }
    if not p then
        return false, "Spawn osascript failed:" .. err
    end
    if p:wait() ~= 0 then
        return false, p.stderr:read "a"
    end
    return true
end

function _M.windows_inject(process, entry)
    local inject = require 'inject'
    if not inject.injectdll(process
            , (WORKDIR / "bin" / "launcher.x86.dll"):string()
            , (WORKDIR / "bin" / "launcher.x64.dll"):string()
            , entry
        ) then
        return false, "injectdll failed."
    end
    return true
end

function _M.inject(process, entry, args)
    if platform_os ~= windows and type(process) == "userdata" then
        process = process:get_id()
    end
    if args.inject == 'lldb' then
        return _M.lldb_inject(process, entry, nil, args.inject_executable)
    elseif args.inject == 'hook' then
        if platform_os == macos then
            if entry == entry_launch or (arch == "arm64" and macos_check_rosetta_process(process)) then
                return _M.lldb_inject(process, entry, nil, args.inject_executable)
            end
            return _M.macos_inject(process, entry)
        elseif platform_os == windows then
            return _M.windows_inject(process, entry)
        else
            return false, ("Inject (use %s) is not supported in %s."):format(args.inject, platform_os)
        end
    else
        return false, ("Inject (use %s) is not supported."):format(args.inject)
    end
end

return _M
