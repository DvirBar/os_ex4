#include <unistd.h>
#include <cmath>
#include <cstring>
#include <sys/mman.h>
#include <time.h>
#include <stdio.h>
#include <iostream>

#define MAX_ORDER 10
#define MIN_BLOCK 128
#define BLOCK_SIZE (MIN_BLOCK*1024)
#define NUM_BLOCKS 32
#define TOTAL_BLOCK_SIZE (BLOCK_SIZE*NUM_BLOCKS)

using namespace std;

struct MallocMetaData {
    int cookie;
    size_t size;
    bool is_free;
    MallocMetaData* next;
    MallocMetaData* prev;
};

int cookie = 0;
bool hasAllocated = false;
MallocMetaData* freeBlocks[MAX_ORDER+1];
size_t numAllocatedBytes = 0;
int numAllocatedBlocks = 0;


void map() {
    MallocMetaData* currentNode;
    int numBlocks = 0;
    printf("----------start------------\n");
    for(int i=0; i<=MAX_ORDER; i++) {
        currentNode = freeBlocks[i];
        while(currentNode != nullptr) {
            numBlocks++;
            currentNode = currentNode->next;
        }

        printf("Order %d: %d\n", i, numBlocks);
        numBlocks = 0;
    }

    printf("-----------finish-----------\n");
}

MallocMetaData* accessMetaData(MallocMetaData* ptr) {
    if(ptr != nullptr && ptr->cookie != cookie) {
        exit(0xdeadbeef);
    }

    return ptr;
}

void setMetaData(MallocMetaData** srcPtr, MallocMetaData* ptrToSet) {
    accessMetaData(*srcPtr);
    accessMetaData(ptrToSet);

    *srcPtr = ptrToSet;
}


//void* findFreeBlock(MallocMetaData* head, size_t size) {
//    MallocMetaData* currentNode = head;
//
//    while(currentNode) {
//        if(currentNode->is_free && currentNode->size <= size) {
//            return currentNode;
//        }
//        currentNode = accessMetaData(currentNode)->next;
//    }
//
//    return (void*)-1;
//}

//void* insertAndAlloc(MallocMetaData* head, size_t size) {
//    MallocMetaData* currentNode = head;
//    while(currentNode->next) {
//        currentNode = currentNode->next;
//    }
//
//    auto newMetaData = (MallocMetaData*)sbrk(sizeof(MallocMetaData));
//    if(newMetaData == (void*)-1) {
//        return (void*)-1;
//    }
//
//    void* pbrk = sbrk(size);
//    if(pbrk == (void*)-1) {
//        return (void*)-1;
//    }
//
//    currentNode->next = newMetaData;
//    newMetaData->is_free = false;
//    newMetaData->size = size;
//    newMetaData->next = nullptr;
//    newMetaData->prev = currentNode;
//
//    return pbrk;
//}

int findMinimalBlock(size_t size, int* pow, int* minBlockSize) {
    *minBlockSize = 1;
    int index = 0;

    while(size > *minBlockSize*MIN_BLOCK-sizeof(MallocMetaData)) {
        index++;
        *minBlockSize*=2;
    }

    *pow = index;

    while(index < MAX_ORDER+1 && accessMetaData(freeBlocks[index]) == nullptr) {
        index++;
    }

    return index;
}

void insertToList(MallocMetaData* addr, int index, size_t size) {
    MallocMetaData* currentNode = accessMetaData(freeBlocks[index]);

    addr->cookie = cookie;
    addr->size = size;

    if(currentNode == nullptr) {
        freeBlocks[index] = addr;
        addr->prev = nullptr;
        addr->next = nullptr;
        return;
    }

    while(accessMetaData(currentNode->next) != nullptr && currentNode < addr) {
        currentNode = currentNode->next;
    }

    accessMetaData(addr->prev);
    accessMetaData(addr->next);

    if(currentNode > addr) {
        if(accessMetaData(currentNode->prev) != nullptr) {
            setMetaData(&currentNode->prev->next, addr);
        } else {
            freeBlocks[index] = addr;
        }

        setMetaData(&addr->prev, currentNode->prev);
        setMetaData(&addr->next,currentNode);
        setMetaData(&currentNode->prev, addr);
    } else {
        setMetaData(&addr->next, nullptr);
        setMetaData(&addr->prev, currentNode);
        accessMetaData(currentNode->next);
        setMetaData(&currentNode->next, addr);
    }
}

void* splitBlock(int currentIndex, int pow) {
    MallocMetaData* leftAddr = accessMetaData(freeBlocks[currentIndex]);
    auto leftAddrNum = (unsigned long)leftAddr;

    freeBlocks[currentIndex] = accessMetaData(leftAddr->next);
    if(leftAddr->next != nullptr)  {
        accessMetaData(leftAddr->next->prev);
        setMetaData(&leftAddr->next->prev, nullptr);
    }

    while(currentIndex > pow) {
        auto rightAddrNum = leftAddrNum+leftAddr->size/2;
        insertToList((MallocMetaData*)rightAddrNum, currentIndex-1, (int)leftAddr->size/2);
        leftAddr->size/=2;
        currentIndex--;
    }

    leftAddr->is_free = false;
    setMetaData(&leftAddr->next, nullptr);
    numAllocatedBytes += leftAddr->size-sizeof(MallocMetaData);
    return leftAddr+1;
}

void* execSmalloc(size_t size) {
    int pow = 0;
    int minBlockSize = 0;
    int index = findMinimalBlock(size, &pow, &minBlockSize);
    if(index > MAX_ORDER) {
        return nullptr;
    }

    return splitBlock(index, pow);
}


void init() {
    for(int i=0; i<MAX_ORDER; i++) {
        freeBlocks[i] = nullptr;
    }

    srand(time(NULL));
    cookie = rand();

    void* initHeapHead = sbrk(0);

    auto initHeapHeadAddr = (unsigned long)initHeapHead;
    unsigned long closestMultiple = initHeapHeadAddr/(TOTAL_BLOCK_SIZE)+1;
    unsigned long diff = ((TOTAL_BLOCK_SIZE)*closestMultiple)-initHeapHeadAddr;
    sbrk(diff);

    auto headPtr = sbrk(TOTAL_BLOCK_SIZE);
    auto headPtrNum = (unsigned long)headPtr;
    freeBlocks[MAX_ORDER] = (MallocMetaData*)headPtr;



    for(unsigned long i=0; i<NUM_BLOCKS; i++) {
        MallocMetaData mData {
            cookie,
            BLOCK_SIZE,
            true,
            (MallocMetaData*)(headPtrNum+((i+1)*BLOCK_SIZE)),
            (MallocMetaData*)(headPtrNum+(i*BLOCK_SIZE))
        };

        *((MallocMetaData*)((unsigned long)freeBlocks[MAX_ORDER]+(i*BLOCK_SIZE))) = mData;
    }

    auto firstBlock = (MallocMetaData*)(headPtrNum);
    firstBlock->prev = nullptr;
    auto lastBlock = (MallocMetaData*)(headPtrNum+((NUM_BLOCKS-1)*BLOCK_SIZE));
    lastBlock->next = nullptr;

    hasAllocated = true;
}

void* smalloc(size_t size) {
    if(size==0 || size>pow(10,8)) {
        return NULL;
    }

    if(!hasAllocated) {
        init();
    }

    void* ptr = NULL;

    if(size < BLOCK_SIZE) {
        ptr = execSmalloc(size);
        if(ptr == nullptr) {
            return NULL;
        }
    } else {
        auto mmapStart = (MallocMetaData*)mmap(NULL, size+sizeof(MallocMetaData), PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        MallocMetaData mData {
            cookie,
            size,
            false,
            nullptr,
            nullptr
        };
        mmapStart[0] = mData;
        ptr = mmapStart+1;
        numAllocatedBytes += size;
    }

    numAllocatedBlocks++;
    return ptr;
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

bool findAndRemove(MallocMetaData* ptr, int index) {
    auto currentNode = accessMetaData(freeBlocks[index]);

    while(accessMetaData(currentNode) != nullptr && currentNode != ptr) {
        setMetaData(&currentNode,currentNode->next);
    }

    if(currentNode != nullptr) {
        accessMetaData(currentNode->next);
        if(accessMetaData(currentNode->prev) != nullptr) {
            accessMetaData(currentNode->prev->next);
            setMetaData(&currentNode->prev->next, currentNode->next);
        } else {
            setMetaData(&freeBlocks[index],currentNode->next);
        }

        if(currentNode->next != nullptr) {
            accessMetaData(currentNode->next->prev);
            setMetaData(&currentNode->next->prev, currentNode->prev);
        }

        return true;
    }

    return false;
}

MallocMetaData* mergeBlocks(MallocMetaData* ptr, int* index, int maxSize) {
    bool buddyFound = false;
    MallocMetaData* lastLeftBlock = ptr;

    do {
        auto buddy = (MallocMetaData*)((unsigned long)lastLeftBlock^(lastLeftBlock->size));
        accessMetaData(buddy);
        buddyFound = findAndRemove(buddy, *index);

        if(buddyFound) {
            if(lastLeftBlock < buddy) {
                lastLeftBlock->size*=2;
            } else {
                buddy->size*=2;
                lastLeftBlock = buddy;
            }
            (*index)++;
        }


    } while (buddyFound && *index < maxSize);
    return lastLeftBlock;
}

void sfree(void* p) {
    if(p == NULL) {
        return;
    }

    int index = 0;
    size_t currentSize = MIN_BLOCK;
    auto ptr = (MallocMetaData*)p;
    ptr--;
    accessMetaData(ptr);
    if(ptr->is_free) {
        return;
    }

    if(ptr->size < BLOCK_SIZE) {
        while(currentSize < ptr->size) {
            currentSize*=2;
            index++;
        }

        size_t prevSize = ptr->size;
        MallocMetaData* blockToInsert = mergeBlocks(ptr, &index, MAX_ORDER);
//        printf("n1 %d, size %d\n", (int)numAllocatedBytes, (int)ptr->size);
        numAllocatedBytes -= prevSize;
        numAllocatedBytes += sizeof (MallocMetaData);
//        printf("n2 %d\n", (int)numAllocatedBytes);
        insertToList(blockToInsert, index, blockToInsert->size);
    } else {
        numAllocatedBytes -= ptr->size;
        cout << ptr->size << endl;
        munmap(ptr, ptr->size+sizeof(MallocMetaData));
        cout << "unmap" << endl;
    }


    numAllocatedBlocks--;
    ptr->is_free = true;
}

bool isMergeable(MallocMetaData* oldp, int index, size_t requestedSize) {
    bool buddyFound = false;
    MallocMetaData* lastLeftBlock = oldp;
    size_t size = lastLeftBlock->size;

    do {
        buddyFound = false;
        auto buddy = (MallocMetaData*)((unsigned long)lastLeftBlock^(size));
        accessMetaData(buddy);
        auto currentNode = accessMetaData(freeBlocks[index]);

        while(accessMetaData(currentNode) != nullptr && currentNode != buddy) {
            setMetaData(&currentNode,currentNode->next);
        }

        if(accessMetaData(currentNode) != nullptr) {
            buddyFound = true;
        }

        if(buddyFound) {
            if(lastLeftBlock >= buddy) {
                lastLeftBlock = buddy;
            }
            size*=2;
        }

        index++;
    } while (buddyFound && size < requestedSize);

    return buddyFound;
}

MallocMetaData* reallocHeap(MallocMetaData* oldp, size_t size) {
    if(oldp->size >= size) {
        return oldp;
    }
    size_t oldpCurrentSize = MIN_BLOCK;
    int oldpIndex = 0;
    size_t currentSize = MIN_BLOCK;
    int index = 0;

    while(oldpCurrentSize < oldp->size) {
        oldpCurrentSize*=2;
        oldpIndex++;
    }

    while(currentSize < size) {
        currentSize*=2;
        index++;
    }

    if(isMergeable(oldp, oldpIndex, size)) {
        numAllocatedBytes -= oldp->size+sizeof(MallocMetaData);

        MallocMetaData* addr = mergeBlocks(oldp, &oldpIndex, index);
        // TODO: nothing is inserted
        numAllocatedBytes += addr->size-sizeof (MallocMetaData);
        return addr+1;
    }

    void* ptr = smalloc(size);
    if(ptr != NULL) {
        sfree(oldp);
    }

    return (MallocMetaData*)ptr;
}


void* srealloc(void* oldp, size_t size) {
    if(oldp == NULL) {
        return smalloc(size);
    }
    MallocMetaData* newPtr;
    // TODO: can srealloc init the data struct when oldp isn't null?
    auto ptr = ((MallocMetaData*)oldp)-1;
    if(size < BLOCK_SIZE) {
        return reallocHeap(ptr, size);
    }

    if(ptr->size == size) {
        return oldp;
    }

    munmap(oldp, ptr->size+sizeof(MallocMetaData));
    newPtr = (MallocMetaData*)mmap(NULL, size+sizeof(MallocMetaData), PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
    numAllocatedBytes -= ptr->size;
    numAllocatedBytes += size;
    return newPtr+1;
}

size_t _num_free_blocks() {
    if(!hasAllocated) {
        return 0;
    }

    MallocMetaData* currentNode;
    size_t numFree = 0;
    for(int i=0; i <= MAX_ORDER; i++) {
        currentNode = freeBlocks[i];
        while(currentNode != nullptr) {
            numFree++;
            currentNode = currentNode->next;
        }
    }

    return numFree;
}

size_t _num_free_bytes() {
    if(!hasAllocated) {
        return 0;
    }

    MallocMetaData* currentNode;
    size_t numFreeBytes = 0;
    for(int i=0; i <= MAX_ORDER; i++) {
        currentNode = freeBlocks[i];
        while(currentNode != nullptr) {
            numFreeBytes += currentNode->size - sizeof(MallocMetaData);
            currentNode = currentNode->next;
        }
    }

    return numFreeBytes;
}

size_t _num_allocated_bytes() {
    return numAllocatedBytes+_num_free_bytes();
}

size_t _num_allocated_blocks() {
    return numAllocatedBlocks+_num_free_blocks();
}

size_t _num_meta_data_bytes() {
    return sizeof(MallocMetaData)*_num_allocated_blocks();
}

size_t _size_meta_data() {
    return sizeof(MallocMetaData);
}




