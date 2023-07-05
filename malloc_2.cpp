#include <unistd.h>
#include <cmath>
#include <cstring>
#include <stdio.h>

using namespace std;

struct MallocMetaData {
    size_t size;
    bool is_free;
    MallocMetaData* next;
    MallocMetaData* prev;
};

void* listHead = sbrk(0);
bool hasAllocated = false;

void* findFreeBlock(MallocMetaData* head, size_t size) {
    MallocMetaData* currentNode = head;

    while(currentNode != nullptr) {
        if(currentNode->is_free && currentNode->size <= size) {
            return currentNode;
        }
        currentNode = currentNode->next;
    }

    return (void*)-1;
}

void* insertAndAlloc(MallocMetaData* head, size_t size) {
    MallocMetaData* currentNode = head;
    while(currentNode->next != nullptr) {
        currentNode = currentNode->next;
    }

    auto newMetaData = (MallocMetaData*)sbrk(sizeof(MallocMetaData));
    if(newMetaData == (void*)-1) {
        return (void*)-1;
    }

    void* pbrk = sbrk(size);
    if(pbrk == (void*)-1) {
        return (void*)-1;
    }

    currentNode->next = newMetaData;
    newMetaData->is_free = false;
    newMetaData->size = size;
    newMetaData->next = nullptr;
    newMetaData->prev = currentNode;

    return pbrk;
}

void* init(size_t size) {
    auto metaBrk = (MallocMetaData*)sbrk(sizeof(MallocMetaData));
    if(metaBrk == (void*)-1) {
        return NULL;
    }

    MallocMetaData newMeta = {
            size,
            false,
            nullptr,
            nullptr
    };

    *(metaBrk) = newMeta;

    void* pbrk = sbrk(size);

    if(pbrk == (void*)-1) {
        return NULL;
    }

    hasAllocated = true;
    return pbrk;
}

void* smalloc(size_t size) {
    if(size==0 || size>pow(10,8)) {
        return NULL;
    }

    if(!hasAllocated) {
        return init(size);
    }

    auto ptr = (MallocMetaData*)findFreeBlock((MallocMetaData*)listHead, size);
    if(ptr != (void*)-1) {
        ptr->is_free = false;
        return ptr + 1;
    }

    void* pbrk = insertAndAlloc((MallocMetaData*)listHead, size);

    if(pbrk == (void*)-1) {
        return NULL;
    }

    return pbrk;
}

void* scalloc(size_t num, size_t size) {
    if(num == 0) {
        return NULL;
    }

    void* pbrk = smalloc(num*size);

    if(pbrk == NULL) {
        return NULL;
    }

    std::memset(pbrk, 0, num*size);

    return pbrk;
}

void sfree(void* p) {
    if(p == NULL) {
        return;
    }

    auto pMeta = ((MallocMetaData*)p-1);
    pMeta->is_free = true;
}


void* srealloc(void* oldp, size_t size) {
    if(oldp == NULL) {
        return smalloc(size);
    }

    // TODO: can srealloc init the data struct when oldp isn't null?

    auto pMeta = ((MallocMetaData*)oldp-1);
    if(size <= pMeta->size) {
        return oldp;
    }

    void* pbrk = smalloc(size);
    if(pbrk == NULL) {
        return NULL;
    }

    std::memmove(pbrk, oldp, pMeta->size);
    pMeta->is_free = true;

    return pbrk;
}

size_t _num_free_blocks() {
    if(!hasAllocated) {
        return 0;
    }

    auto head = (MallocMetaData*)listHead;
    auto currentNode = head;
    size_t counter = 0;

    while(currentNode != nullptr) {
        if(currentNode->is_free) {
            counter++;
        }

        currentNode = currentNode->next;
    }

    return counter;
}

size_t _num_free_bytes() {
    if(!hasAllocated) {
        return 0;
    }
    auto head = (MallocMetaData*)listHead;
    auto currentNode = head;
    size_t numBytes = 0;

    while(currentNode != nullptr) {
        if(currentNode->is_free) {
            numBytes+=currentNode->size;
        }

        currentNode = currentNode->next;
    }

    return numBytes;
}

size_t _num_allocated_bytes() {
    if(!hasAllocated) {
        return 0;
    }
    auto head = (MallocMetaData*)listHead;
    auto currentNode = head;
    size_t numBytes = 0;

    while(currentNode != nullptr) {
        numBytes+=currentNode->size;
        currentNode = currentNode->next;
    }

    return numBytes;
}

size_t _num_allocated_blocks() {
    if(!hasAllocated) {
        return 0;
    }
    auto head = (MallocMetaData*)listHead;
    auto currentNode = head;
    size_t counter = 0;

    while(currentNode != nullptr) {
        printf("%ld", (unsigned long)currentNode);
        counter++;
        currentNode = currentNode->next;
    }

    return counter;
}

size_t _num_meta_data_bytes() {
    return sizeof(MallocMetaData)*_num_allocated_blocks();
}

size_t _size_meta_data() {
    return sizeof(MallocMetaData);
}

//int main() {
//    void* ptr = smalloc(10);
//    _num_allocated_blocks();
//    _num_allocated_bytes();
//    _num_free_blocks();
//    _num_free_bytes();
//    _num_meta_data_bytes();
//    sfree(ptr);
//}


