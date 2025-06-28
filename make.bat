git submodule update --init --recursive

call compile\windows\init_luamake.bat
if not exist luamake\luamake.exe (
    pushd luamake
    call compile\build.bat
    popd
)

call luamake\luamake.exe lua compile/download_deps.lua
IF "%~1"=="" (
    call luamake\luamake.exe build --debug
) ELSE (
    call luamake\luamake.exe build --platform %1
)