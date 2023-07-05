#ifndef HW4_MALLOCMETADATA_H
#define HW4_MALLOCMETADATA_H

#include <unistd.h>


struct MallocMetaData {
    size_t size;
    bool is_free;
    MallocMetaData* next;
    MallocMetaData* prev;
};


#endif //HW4_MALLOCMETADATA_H
