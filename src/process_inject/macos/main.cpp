#include <string.h>

#include "injectdll.h"

int main(const int argc, const char* argv[]) {
    if (argc < 4) {
        return -1;
    }
    if (!argv[1]) return 1;
    if (!argv[2]) return 2;
    if (!argv[3]) return 3;
    int base = 10;
    if (argv[1][0] == '0' && argv[1][1] == 'x') {
        base = 16;
    }
    pid_t pid = (pid_t)strtoull(argv[1], nullptr, base);
    return injectdll(pid, argv[2], argv[3]) ? 0 : 4;
}