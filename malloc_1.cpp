#include <unistd.h>
#include <cmath>

void* smalloc(size_t size) {
    if(size==0 || size>pow(10,8)) {
        return nullptr;
    }

    void* pbreak = sbrk(size);
    if(pbreak == (void*)-1) {
        return nullptr;
    }

    return pbreak;
}