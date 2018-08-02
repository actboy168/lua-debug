local src = root .. "src/debugger/"
target("debugger")
    set_kind("shared")
    add_deps("lua53-dll")
    set_languages("cxx17")
    add_cxxflags("-DRAPIDJSON_HAS_STDSTRING")
    if is_plat("windows") then
        if is_mode("release") then
            add_cxxflags("-MD")
            add_cxxflags("-GL")
            add_cxxflags("-Oi")
            --add_cxxflags("-Zi")
            add_ldflags("-OPT:ICF", "-OPT:REF", "-SAFESEH", "-INCREMENTAL:NO")
        else
            add_cxxflags("-MDd")
            add_cxxflags("-RTC1")
            --add_cxxflags("-ZI")
        end
        add_cxxflags("-EHsc", "-FC", "-fp:precise", "-Zc:inline", "-Zc:wchar_t", "-Zc:forScope")
        add_cxxflags("-Oy-")
        add_cxxflags("-Gd", "-Gy", "-GS", "-Gm-")
        add_cxxflags("-DDEBUGGER_EXPORTS", "-D_WIN32_WINNT=0x0600")
        add_links("ws2_32")
        add_ldflags("-TLBID:1", "-NXCOMPAT", "-DYNAMICBASE")
    elseif is_plat("mingw") then
        add_cxxflags("-DDEBUGGER_EXPORTS", "-D_WIN32_WINNT=0x0600")
        add_ldflags("-s")
        add_links("ws2_32")
    else
        add_links("pthread")
    end
    add_includedirs(root .. "include/")
    add_includedirs(root .. "third_party/")
    add_includedirs(root .. "third_party/lua53/")
    add_includedirs(root .. "third_party/readerwriterqueue/")
    add_files(src .. "*.cpp")
    add_files(src .. "io/*.cpp")
    add_files(src .. "bridge/luaopen.cpp")
target_end()
