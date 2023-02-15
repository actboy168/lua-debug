local platform_os = require 'frontend.platform_os'
local log = require 'common.log'
local fs = require 'bee.filesystem'
local sp = require 'bee.subprocess'

return function(process, entry)
    if platform_os() == 'macOS' then
        if type(process) == "userdata" then
            process = process:get_id()
        end
        local helper = (WORKDIR / "bin" / "process_inject_helper"):string()
        local dylib = (WORKDIR / "bin" / "launcher.so"):string()
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
            return false, "Spawn osascript failed:"..err
        end
        if p:wait() ~= 0 then
            return false, p.stderr:read "a"
        end
        return true
    elseif platform_os() == 'Windows' then
        local inject = require 'inject'
        if not inject.injectdll(process
            , (WORKDIR / "bin" / "launcher.x86.dll"):string()
            , (WORKDIR / "bin" / "launcher.x64.dll"):string()
            , entry
        ) then
            return false, "injectdll failed."
        end
    else
        return false, "Inject unsupported."
    end
end