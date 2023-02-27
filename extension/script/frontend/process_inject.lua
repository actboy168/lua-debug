local fs = require 'bee.filesystem'
local sp = require 'bee.subprocess'
local arch = require "bee.platform".Arch
local platform_os = require 'frontend.platform_os' ()

local _M = {}

local macos = "macOS"
local windows = "Windows"

function _M.get_inject_library_path()
    if platform_os == macos then
        return (WORKDIR / "bin" / "launcher.so"):string()
    end
end

function _M.lldb_inject(pid, entry, injectdll, lldb_path)
    lldb_path = lldb_path or "lldb"
    local p, err = sp.spawn {
        lldb_path,
        "-p", pid,
        "--batch",
        "-o",
        -- 6 = RTDL_NOW|RTDL_LOCAL
        ('expression (void*)dlopen("%s", 6)'):format(injectdll),
        "-o",
        ('expression ((void(*)())&%s)()'):format(entry),
        "-o",
        "quit",
        stdout = true,
        stderr = true,
    }
    if not p then
        return false, "Spwan lldb failed:" .. err
    end
    if p:wait() ~= 0 then
        return false, "stdout:" .. p.stdout:read "a" .."\nstderr:" .. p.stderr:read "a"
    end
    return true
end

function _M.macos_check_rosetta_process(process)
    local rosetta_runtime = "/usr/libexec/rosetta/runtime"
    if not fs.exists(rosetta_runtime) then
        return false
    end
    local p, err = sp.spawn({
        "/usr/bin/fuser",
        rosetta_runtime,
        stdout = true,
        stderr = true, -- for skip  fuser output
    })
    if not p then
        return false
    end
    local l = p.stdout:read("a")
    return l:find(tostring(process)) ~= nil
end

function _M.macos_inject(process, entry, dylib)
    local helper = (WORKDIR / "bin" / "process_inject_helper"):string()
    if not fs.exists(dylib) then
        return false, "Not found launcher.so."
    end
    local p, err = sp.spawn {
        "/usr/bin/osascript",
        "-e",
        ([[do shell script "%s %d %s %s" with administrator privileges with prompt "lua-debug"]]):format(helper, process, dylib, entry),
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
        return _M.lldb_inject(process, entry, _M.get_inject_library_path(), args.inject_executable)
    else
        if platform_os == macos then
            if arch == "arm64" then
                if _M.macos_check_rosetta_process(process) then
                    return _M.lldb_inject(process, entry, _M.get_inject_library_path(), args.inject_executable)
                end
            end
            return _M.macos_inject(process, entry, _M.get_inject_library_path())
        elseif platform_os == windows then
            return _M.windows_inject(process, entry)
        else
            return false, "Inject unsupported."
        end
    end
end

return _M
