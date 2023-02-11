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
            return false
        end
        local p, err = sp.spawn {
            "/usr/bin/osascript",
            "-e",
            ([[do shell script "%s %d %s %s" with administrator privileges with prompt "lua-debug"]]):format(helper, process, dylib, entry),
            stderr = true,
        }
        if not p then
            log.error("spawn osascript failed", err)
            return false
        end
		if p:wait() ~= 0 then
			log.error(p.stderr:read "a")
			return false
		end
		return true
    else
        local inject = require 'inject'
        if platform_os() == 'Windows' then
            if not inject.injectdll(process
            , (WORKDIR / "bin" / "windows" / "launcher.x86.dll"):string()
            , (WORKDIR / "bin" / "windows" / "launcher.x64.dll"):string()
            , entry
        ) then
            return false
        end
        else
			--TODO:LINUX

        end
    end
end