root = "../../"
set_warnings("all")
if is_mode("release") then
    if is_plat("windows") then
        add_cxxflags("-O2")
    else
        set_optimize("faster")
    end
    set_symbols("hidden")
    set_strip("all")
else
    set_optimize("none")
    set_symbols("debug")
end

includes 'xmake/lua53.lua'
includes 'xmake/lua54.lua'
includes 'xmake/debugger.lua'
