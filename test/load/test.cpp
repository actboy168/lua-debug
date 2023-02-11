#include <stdio.h>
#include <dlfcn.h>

int main(int argc, char *argv[]) {
    for (size_t i = 1; i < argc; i++)
    {
        const char* path = argv[i];
        if(!dlopen(path, RTLD_NOW|RTLD_GLOBAL)) {
            fprintf(stderr,"load %s failed: %s\n", path, dlerror());
            return 1;
        }
    }
    return 0;    
}