#include <stdlib.h>
#include <filesystem>

extern "C" {
void attach() {
    std::filesystem::remove(std::filesystem::current_path() / "test/inject/launcher.so");
    exit(0);
}
}
