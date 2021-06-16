//
// Created by admin on 4/4/2020.
//

#ifndef MODULE_MEM_H
#define MODULE_MEM_H

typedef struct MemoryAreaSt {
    int startAddr;
    int endAddr;
    int curOffset;  //-- where in this range we can allocate memory from next
} MemoryArea;

typedef struct {
    int addr;
    MemoryArea *memoryArea;
} MemoryAllocation;

// Old method for creating the ranges
extern MemoryArea *createMemoryRange (int start, int end);

//--------------------------------------------------------------
// New methods for managing memory

extern void SMA_init(int startArea);
extern MemoryArea *SMA_getZeropageArea();
extern MemoryArea *SMA_addMemoryRange (int start, int end);
extern MemoryAllocation SMA_allocateMemory(MemoryArea *specificMemArea, int size);

#endif //MODULE_MEM_H
