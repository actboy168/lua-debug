git submodule update --init --recursive
cd 3rd\luamake
call compile\build.bat
cd ..\..
call 3rd\luamake\luamake.exe lua compile/download_deps.lua
IF "%~1"=="" (
    call 3rd\luamake\luamake.exe build --debug
) ELSE (
    call 3rd\luamake\luamake.exe build --platform %1
)