local platform = require 'bee.platform'
local fs = require "bee.filesystem"
local sp = require 'bee.subprocess'

local output_dir = "3rd/frida_gum/"

local version = "16.0.10"
local file_fmt = "%s-%s." .. (platform.os == "windows" and "exe" or "tar.xz")
local url_fmt = ("https://github.com/frida/frida/releases/download/%s/frida-gum-devkit-%s")

local all_os = {
    "macos-arm64",
    "macos-x86_64",
    "windows-x86",
    "windows-x86_64",
    "linux-x86_64",
    "linux-arm64",
    --"linux-x86",
    --"freebsd-arm64",
    --"freebsd-x86_64",
}

---@param url string
---@param output string
---@param dir string
local function download(url, output, dir)
    local downloader = platform.os == "macos" and { "curl", "--location", "-C", "-" } or "wget"
    local wget = {
        downloader,
        url,
        downloader == "wget" and "-O" or "-o",
        output
    }
    local tar = {
        "tar",
        "-xvf",
        output,
        "-C",
        dir,
    }
    local cmds = { wget, tar }

    if platform.os == "windows" then
        cmds[#cmds] = {
            output,
            "x",
            "-y",
            "-o" .. dir
        }
        for _, cmd in ipairs(cmds) do
            local ok, err = os.execute("powershell -Command " .. table.concat(cmd, " "))
            if not ok then
                error(err)
            end
        end
        return
    end
    for _, cmd in ipairs(cmds) do
        local p, err = sp.spawn(cmd)
        if not p then
            error(cmd[1] .. ":" .. err)
        end
        p:wait()
    end
end

local targets = {}
for _, os in ipairs(all_os) do
    if os:find(platform.os, 0, true) then
        table.insert(targets, os)
    end
end

for index, os in ipairs(targets) do
    local file = file_fmt:format(version, os)
    local url = url_fmt:format(version, file)
    file = output_dir .. file
    print(("[%d/%d]"):format(index, #targets), url, file)
    local target_dir = output_dir .. os
    fs.create_directory(target_dir)
    local target_file = fs.path(target_dir) / "frida-gum.h"
    if not fs.exists(target_file) then
        download(url, file, target_dir)
    else
        print("use cached ", target_file)
    end
end
