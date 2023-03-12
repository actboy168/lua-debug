#define _CRT_SECURE_NO_WARNINGS 1
#include <Windows.h>
#include <stdio.h>
#include <string>
#include <optional>
#include <filesystem>

std::optional<std::filesystem::path> get_exe_path() {
	wchar_t buffer[MAX_PATH];
	DWORD path_len = ::GetModuleFileNameW(NULL, buffer, _countof(buffer));
	if (path_len == 0) {
		return std::nullopt;
	}
	if (path_len < _countof(buffer)) {
		return std::filesystem::path(buffer, buffer + path_len);
	}
	return std::nullopt;
}

int main(int narg, const char *args[])
{
    if (narg != 2)
        return -1;
    auto handler = LoadLibraryA(args[1]);
    if (!handler)
        return GetLastError();
    auto fn = GetProcAddress(handler, "inject");
    if (!fn)
        return 2;
    std::string code;
    auto ptr = (unsigned char *)fn;
    while (*ptr != 0xcc)
    {
        char buff[64];
        sprintf_s(buff, "\"\\x%02x\"", *ptr);
        code.append(buff);
        ptr++;
    }
    auto path = get_exe_path();
    if (!path)
        return 3;
    auto output = path.value().replace_filename("shellcode.inl");
    auto file = fopen(output.generic_string().c_str(), "w+");
    fprintf_s(file, "%s", code.c_str());
    return 0;
}