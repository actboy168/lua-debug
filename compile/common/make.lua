local lm = require 'luamake'

lm:copy 'copy_json' {
    input = '3rd/json.lua/json.lua',
    output = 'publish/script/common/json.lua'
}

lm:copy 'copy_bootstrap' {
    input = 'extension/script/bootstrap.lua',
    output = 'publish/bin/main.lua',
}

lm:build 'copy_extension' {
    '$luamake', 'lua', 'compile/copy_extension.lua',
}

lm:build 'update_version' {
    '$luamake', 'lua', 'compile/update_version.lua',
}

lm:build 'package_json' {
    '$luamake', 'lua', 'compile/common/write_json.lua', '$in', '$out', lm.platform,
    input = "compile/common/package_json.lua",
    output = "publish/package.json",
}

lm:phony 'common' {
    deps = {
        'copy_json',
        'copy_bootstrap',
        'copy_extension',
        'update_version',
        'package_json',
    }
}
