local platform_os = require 'frontend.platform_os'
local log = require 'common.log'
local fs = require 'bee.filesystem'

return function(pid, entry)
	if platform_os() == 'macOS' then
        local helper = (WORKDIR / "bin" / "macos" / "process_inject_helper"):string()
        local dylib = (WORKDIR / "bin" / "macos" / "launcher" .. ".so"):string()
        if not fs.exists(dylib) then
            return false
        end
		local shell = ([[/usr/bin/osascript -e "do shell script \"%s %d %s %s\" with administrator privileges with prompt \"lua-debug\""]]):format(helper, pid, dylib, entry)
		local helper_proc, err = io.popen(shell, "r")
		if not helper_proc then
			log.error(err)
			return false
		end
		err = helper_proc:read("a")
		if err ~= '\n' then
			log.error(err)
			return false
		end
		return true
    else
        local inject = require 'inject'
        if platform_os() == 'Windows' then
            if not inject.injectdll(pid
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