local lm = require 'luamake'

lm:copy 'copy_json' {
    input = '3rd/json.lua/json.lua',
    output = 'publish/script/common/json.lua'
}

lm:copy 'copy_bootstrap' {
    input = 'extension/script/bootstrap.lua',
    output = 'publish/bin/main.lua',
}

lm:runlua 'update_version' {
    script = 'compile/update_version.lua',
}

lm:runlua 'package_json' {
    script = 'compile/common/write_json.lua',
    args = {'$in', '$out', lm.platform},
    input = "compile/common/package_json.lua",
    output = "extension/package.json",
}

lm:runlua 'copy_extension' {
    deps = 'package_json',
    script = 'compile/copy_extension.lua',
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
