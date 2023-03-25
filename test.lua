local os = require "bee.platform".os
if os == "macos" then
    require "test.load.test_macos"
    require "test.inject.inject_macos"
    require "test.waitdll"
end

require "test.interceptor"
