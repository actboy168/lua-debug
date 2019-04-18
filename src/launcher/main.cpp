#include <bee/filesystem.h>

static void initialize(const char* name) {
}

extern "C" __declspec(dllexport)
void __cdecl launch() {
    initialize("launch");
}

extern "C" __declspec(dllexport)
void __cdecl attach() {
    initialize("attach");
}
