#include "MallocMetaData.h"


void* findFreeBlock(MallocMetaData* head, size_t size) {
    MallocMetaData* currentNode = head;

    while(currentNode) {
        if(currentNode->is_free && currentNode->size < size) {
            return currentNode;
        }
        currentNode = currentNode->next;
    }

    return (void*)-1;
}