local src = root .. "third_party/lua53/"
target("lua53")
    set_kind("shared")
    set_basename("lua53")
    if is_plat("windows") then
        add_cflags("-DLUA_BUILD_AS_DLL")
    elseif is_plat("mingw") then
        add_cflags("-std=gnu99", "-Wall", "-Wextra", "-DLUA_BUILD_AS_DLL")
        add_ldflags("-s")
    else
        add_cflags("-std=gnu99", "-Wall", "-Wextra", "-DLUA_USE_LINUX")
    end
    add_files(src .. "*.c")
    del_files(src .. "lua.c")
    del_files(src .. "luac.c")
target_end()

target("lua53-exe")
    set_kind("binary")
    set_basename("lua53")
    if is_plat("windows") then
        add_cflags("-DLUA_BUILD_AS_DLL")
    elseif is_plat("mingw") then
        add_cflags("-std=gnu99", "-Wall", "-Wextra", "-DLUA_BUILD_AS_DLL")
        add_ldflags("-s")
    else
        add_cflags("-std=gnu99", "-Wall", "-Wextra", "-DLUA_USE_LINUX")
        add_links("dl", "readline")
    end
    add_deps("lua53")
    add_files(src .. "lua.c")
target_end()
