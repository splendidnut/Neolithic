//
// Created by admin on 4/4/2020.
//

#ifndef MODULE_MEM_H
#define MODULE_MEM_H

typedef struct MemoryRangeStructure {
    int startAddr;
    int endAddr;
} MemoryRange;


extern MemoryRange *createMemoryRange (int start, int end);

#endif //MODULE_MEM_H
