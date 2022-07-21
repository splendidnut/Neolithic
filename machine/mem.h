/***************************************************************************
 * Neolithic Compiler - Simple C Cross-compiler for the 6502
 *
 * Copyright (c) 2020-2022 by Philip Blackman
 * -------------------------------------------------------------------------
 *
 * Licensed under the GNU General Public License v2.0
 *
 * See the "LICENSE.TXT" file for more information regarding usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * -------------------------------------------------------------------------
 */

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


//--------------------------------------------------------------
// Methods for managing memory

extern void SMA_init(int startArea);
extern MemoryArea *SMA_getZeropageArea();
extern MemoryArea *SMA_addMemoryRange (int start, int end);
extern MemoryAllocation SMA_allocateMemory(MemoryArea *specificMemArea, int size);

#endif //MODULE_MEM_H
