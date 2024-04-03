local _LOADED = {}
local _PRELOAD = package.preload
local PATH = package.path

local function searcher_preload(name)
    local func = _PRELOAD[name]
    if func then
        return func
    end
end

local function searcher_lua(name)
    local filename = name:gsub('%.', '/')
    local path = PATH:gsub('%?', filename)
    local f = io.open(path)
    if f then
        local func, err = load(f:read "a", "@"..path)
        f:close()
        if not func then
            error(("error loading module '%s' from file '%s':\n\t%s"):format(name, path, err))
        end
        return func
    end
end

function require(name)
    local _ = type(name) == "string" or error (("bad argument #1 to 'require' (string expected, got %s)"):format(type(name)))
    local p = _LOADED[name]
    if p ~= nil then
        return p
    end
    local initfunc = searcher_preload(name)
    if initfunc then
        local r = initfunc()
        if r == nil then
            r = true
        end
        _LOADED[name] = r
        return r
    end
    initfunc = searcher_lua(name)
    if initfunc then
        local r = initfunc()
        if r == nil then
            r = true
        end
        _LOADED[name] = r
        return r
    end
    local filename = name:gsub('%.', '/')
    local path = PATH:gsub('%?', filename)
    error(("module '%s' not found:\n\tno field package.preload['%s']\n\tno file '%s'"):format(name, name, path))
end

