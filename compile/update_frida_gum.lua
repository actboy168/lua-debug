local platform =  require 'bee.platform'
if platform == "windows" then
    error("Windows is not supported")
end
local sp = require 'bee.subprocess'

local output_dir = "3rd/frida_gum/"

local version = "16.0.10"
local file_fmt = "%s.tar.xz"
local url_fmt = ("https://github.com/frida/frida/releases/download/%s/frida-gum-devkit-%s-%s")

local all_os = {
    "macos-arm64",
    "macos-x86_64",
    "windows-x86",
    "windows-x86_64",
    "linux-x86_64",
    "linux-arm64",
    "linux-x86",
    "freebsd-arm64",
    "freebsd-x86_64",
}

local function download(url, output, dir)
    local p, err = sp.spawn({
        "wget",
        url,
        "-O",
        output
    })
    if not p then
        return
    end
    if p:wait() ~= 0 then
        return
    end
    dir = output_dir..dir
    p = sp.spawn({
        "mkdir",
        dir
    })
    if not p then
        return
    end
    p:wait()
    p = sp.spawn({
        "tar",
        "-xvf",
        output,
        "-C",
        dir,
    })
    if p then
        p:wait()
    end
end

for index, os in ipairs(all_os) do
    local file = file_fmt:format(os)
    local url = url_fmt:format(version, version, file)
    file = output_dir..file
    print(("%d/%d"):format(index, #all_os), url, file)
    download(url, file, os)
end
