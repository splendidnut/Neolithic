//
//  Static Memory Allocator -  Variable memory management
//
//  Used for tracking available memory ranges
//    and memory available for variable allocations.
//
// Created by admin on 4/4/2020.
//

#include <stdio.h>
#include "mem.h"

//-------------------------------------
//  Memory management variables

static MemoryArea memoryAreas[16];
static int cntMemoryAreas;
static int firstMemAllocArea;       // memory allocation area to use first

//--------------------------------------------------------------------------------------

void SMA_init(int startArea) {
    cntMemoryAreas = 0;
    firstMemAllocArea = startArea;
}

MemoryArea *SMA_getZeropageArea() {
    return &memoryAreas[0];
}

MemoryArea *SMA_addMemoryRange(int start, int end) {
    // TODO: Maybe do this a different way?
    //       Might not matter since this compiler is currently 6502 specific.

    // Make sure memoryArea[0] is zeropage range... (6502 specific)
    if ((cntMemoryAreas == 0) && (end > 255)) {
        printf("ERROR: MemoryArea[0] should be zeropage!\n");
    }

    MemoryArea *curRange = &memoryAreas[cntMemoryAreas];
    curRange->startAddr = start;
    curRange->endAddr = end;
    curRange->curOffset = start;
    cntMemoryAreas++;
    return curRange;
}


MemoryAllocation SMA_allocateMemory(MemoryArea *specificMemArea, int size) {
    MemoryAllocation resultMemAlloc;

    // if user specified memory area to use, attempt to allocate there first
    if (specificMemArea != NULL) {
        int memoryLoc = specificMemArea->curOffset;

        // can it fit in user-specified space?
        if (memoryLoc+size < specificMemArea->endAddr) {
            resultMemAlloc.memoryArea = specificMemArea;
            resultMemAlloc.addr = memoryLoc;
            specificMemArea->curOffset += size;
            return resultMemAlloc;
        }
    }

    // if user didn't specify, or no room there, start at first area specified...
    int curRangeIdx = firstMemAllocArea;
    MemoryArea *curRange;

    // loop thru all memory areas, looking for space
    while (curRangeIdx < cntMemoryAreas) {
        curRange = &memoryAreas[curRangeIdx];
        int memoryLoc = curRange->curOffset;

        // can it fit?
        if ((memoryLoc + size - 1) <= curRange->endAddr) {
            resultMemAlloc.memoryArea = curRange;
            resultMemAlloc.addr = memoryLoc;
            curRange->curOffset += size;
            return resultMemAlloc;
        }
        curRangeIdx++;
    }

    // error if out of memory
    resultMemAlloc.memoryArea = NULL;
    resultMemAlloc.addr = -1;
    return resultMemAlloc;
}