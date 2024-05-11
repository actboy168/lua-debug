local socket = require "bee.socket"
local epoll = require "bee.epoll"
local fs = require "bee.filesystem"

local epfd = epoll.create(512)

local EPOLLIN <const> = epoll.EPOLLIN
local EPOLLOUT <const> = epoll.EPOLLOUT
local EPOLLERR <const> = epoll.EPOLLERR
local EPOLLHUP <const> = epoll.EPOLLHUP

local function fd_set_read(s)
    if s._flags & EPOLLIN ~= 0 then
        return
    end
    s._flags = s._flags | EPOLLIN
    epfd:event_mod(s._fd, s._flags)
end

local function fd_clr_read(s)
    if s._flags & EPOLLIN == 0 then
        return
    end
    s._flags = s._flags & (~EPOLLIN)
    epfd:event_mod(s._fd, s._flags)
end

local function fd_set_write(s)
    if s._flags & EPOLLOUT ~= 0 then
        return
    end
    s._flags = s._flags | EPOLLOUT
    epfd:event_mod(s._fd, s._flags)
end

local function fd_clr_write(s)
    if s._flags & EPOLLOUT == 0 then
        return
    end
    s._flags = s._flags & (~EPOLLOUT)
    epfd:event_mod(s._fd, s._flags)
end

local function on_event(self, name, ...)
    local f = self._event[name]
    if f then
        return f(self, ...)
    end
end

local function close(self)
    local fd = self._fd
    on_event(self, "close")
    epfd:event_del(fd)
    fd:close()
end

local stream_mt = {}
local stream = {}
stream_mt.__index = stream
function stream_mt:__newindex(name, func)
    if name:sub(1, 3) == "on_" then
        self._event[name:sub(4)] = func
    end
end

function stream:write(data)
    if self.shutdown_w then
        return
    end
    if data == "" then
        return
    end
    if self._writebuf == "" then
        fd_set_write(self)
    end
    self._writebuf = self._writebuf .. data
end

function stream:is_closed()
    return self.shutdown_w and self.shutdown_r
end

function stream:close()
    if not self.shutdown_r then
        self.shutdown_r = true
        fd_clr_read(self)
    end
    if self.shutdown_w or self._writebuf == "" then
        self.shutdown_w = true
        fd_clr_write(self)
        close(self)
    end
end

local function close_write(self)
    fd_clr_write(self)
    if self.shutdown_r then
        self.shutdown_w = true
        close(self)
    end
end
local function update_stream(s, event)
    if event & (EPOLLERR | EPOLLHUP) ~= 0 then
        event = event & (EPOLLIN | EPOLLOUT)
    end
    if event & EPOLLIN ~= 0 then
        local data = s._fd:recv()
        if data == nil then
            s:close()
        elseif data == false then
        else
            on_event(s, "data", data)
        end
    end
    if event & EPOLLOUT ~= 0 then
        local n = s._fd:send(s._writebuf)
        if n == nil then
            s.shutdown_w = true
            close_write(s)
        elseif n == false then
        else
            s._writebuf = s._writebuf:sub(n + 1)
            if s._writebuf == "" then
                close_write(s)
            end
        end
    end
end

local listen_mt = {}
local listen = {}
listen_mt.__index = listen
function listen_mt:__newindex(name, func)
    if name:sub(1, 3) == "on_" then
        self._event[name:sub(4)] = func
    end
end

function listen:is_closed()
    return self.shutdown_r
end

function listen:close()
    self.shutdown_r = true
    close(self)
end

local connect_mt = {}
local connect = {}
connect_mt.__index = connect
function connect_mt:__newindex(name, func)
    if name:sub(1, 3) == "on_" then
        self._event[name:sub(4)] = func
    end
end

function connect:write(data)
    if data == "" then
        return
    end
    self._writebuf = self._writebuf .. data
end

function connect:is_closed()
    return self.shutdown_w
end

function connect:close()
    self.shutdown_w = true
    close(self)
end

local m = {}

function m.listen(protocol, address, port)
    local fd; do
        local err
        fd, err = socket.create(protocol)
        if not fd then
            return nil, err
        end
        if protocol == "unix" then
            fs.remove(address)
        end
    end
    do
        local ok, err = fd:bind(address, port)
        if not ok then
            fd:close()
            return nil, err
        end
    end
    do
        local ok, err = fd:listen()
        if not ok then
            fd:close()
            return nil, err
        end
    end
    local s = {
        _fd = fd,
        _flags = EPOLLIN,
        _event = {},
        shutdown_r = false,
        shutdown_w = true,
    }
    epfd:event_add(fd, EPOLLIN, function()
        local new_fd, err = fd:accept()
        if new_fd == nil then
            s:close()
            on_event(s, "error", err)
            return
        elseif new_fd == false then
        else
            local new_s = setmetatable({
                _fd = new_fd,
                _flags = EPOLLIN,
                _event = {},
                _writebuf = "",
                shutdown_r = false,
                shutdown_w = false,
            }, stream_mt)
            if on_event(s, "accepted", new_s) then
                epfd:event_add(new_fd, new_s._flags, function(event)
                    update_stream(new_s, event)
                end)
            else
                new_s:close()
            end
        end
    end)
    return setmetatable(s, listen_mt)
end

function m.connect(protocol, address, port)
    local fd; do
        local err
        fd, err = socket.create(protocol)
        if not fd then
            return nil, err
        end
    end
    do
        local ok, err = fd:connect(address, port)
        if ok == nil then
            fd:close()
            return nil, err
        end
    end
    local s = {
        _fd = fd,
        _flags = EPOLLOUT,
        _event = {},
        _writebuf = "",
        shutdown_r = false,
        shutdown_w = false,
    }
    epfd:event_add(fd, EPOLLOUT, function()
        local ok, err = fd:status()
        if ok then
            on_event(s, "connected")
            setmetatable(s, stream_mt)
            if s._writebuf ~= "" then
                update_stream(s, EPOLLOUT)
                if s._writebuf ~= "" then
                    s._flags = EPOLLIN | EPOLLOUT
                else
                    s._flags = EPOLLIN
                end
            else
                s._flags = EPOLLIN
            end
            epfd:event_mod(s._fd, s._flags, function(event)
                update_stream(s, event)
            end)
        else
            s:close()
            on_event(s, "error", err)
        end
    end)
    return setmetatable(s, connect_mt)
end

local tasks = {}

function m.async(func)
    tasks[#tasks+1] = func
end

function m.add_fd(fd, event, func)
    epfd:event_add(fd, event, func)
end

function m.update(timeout)
    if #tasks > 0 then
        local t = tasks
        tasks = {}
        for i = 1, #t do
            t[i]()
        end
    end
    for func, event in epfd:wait(timeout) do
        func(event)
    end
end

return m

