//
// Created by admin on 4/4/2020.
//

#include <stdlib.h>
#include "mem.h"


MemoryRange *createMemoryRange(int start, int end) {
    MemoryRange *newMem = malloc(sizeof(struct MemoryRangeStructure));
    newMem->startAddr = start;
    newMem->endAddr = end;
    return newMem;
}
