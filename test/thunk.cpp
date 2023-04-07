#include "thunk/thunk.h"

#include <assert.h>
struct HHI {
    int a = 0x1;
};

int add(HHI* h, void* a, void* b) {
    h->a = 2;
    assert(a == (void*)0x1);
    assert(b == (void*)0x2);
    return 1;
}

void* add1(HHI* h, void* a, size_t b, size_t c) {
    h->a = 3;
    assert(a == (void*)0x1);
    assert(b == 2);
    assert(c == 3);
    return nullptr;
}

int main() {
    HHI hhi;
    auto* thunk = thunk_create_hook((intptr_t)&hhi, (intptr_t)&add);
    int ret     = ((int (*)(void* a, void* b))thunk->data)((void*)0x1, (void*)0x2);
    assert(hhi.a == 2);
    assert(ret == 1);

    auto* thunk1 = thunk_create_allocf((intptr_t)&hhi, (intptr_t)&add1);
    auto ret1    = ((void* (*)(void* a, size_t b, size_t c))thunk1->data)((void*)0x1, 2, 3);
    assert(hhi.a == 3);
    assert(ret1 == nullptr);

    return 0;
}