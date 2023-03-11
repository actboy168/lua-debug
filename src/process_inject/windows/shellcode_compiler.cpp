#include <Windows.h>
#include <stdio.h>

int main(int narg, const char* args[]){
    if (narg != 2)
        return -1;
    auto handler = LoadLibraryA(args[1]);
    if (!handler)
        return 1;
    auto fn = GetProcAddress(handler, "inject");
    if (!fn)
        return 2;
    auto ptr = (unsigned char*)fn;
    while (*ptr != 0xcc) {
        char buff[3];
        sprintf_s(buff, "%02x", *ptr);
        fprintf_s(stdout, "%s", buff);
        ptr++;
    }
    return 0;
}