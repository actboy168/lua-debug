#include <stdlib.h>
#include <filesystem>
#include <thread>
#include <chrono>

extern "C" {
void attach() {
    std::thread([](){
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1s);
        std::filesystem::remove(std::filesystem::current_path() / "test/inject/launcher.so");
        exit(0);
    }).detach();
}
}
