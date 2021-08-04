//
// Created by User on 8/4/2021.
//

//===========================================================================
//  deal with machine specific stuff

#include <stdio.h>
#include "mem.h"
#include "machine.h"

void prepForMachine(enum Machines machine) {
    MemoryArea *zeropageMem;
    switch (machine) {
        case Atari2600:
            SMA_init(0);        // only has zeropage memory
            zeropageMem = SMA_addMemoryRange(0x80, 0xFF);

            break;
        case Atari7800:
            // atari7800 memory ranges:
            //    zeropage = 0x40..0xFF
            //      stack  = 0x140..0x1FF
            //    mem1     = 0x1800..0x1FFF     -- 2k
            //    mem2     = 0x2200..0x27FF     -- 1.5k

            SMA_init(1);        // save zeropage memory for other things
            zeropageMem = SMA_addMemoryRange(0x40, 0xFF);
            SMA_addMemoryRange(0x1800, 0x1FFF);
            SMA_addMemoryRange(0x2200, 0x27FF);
            break;
        default:
            printf("Unknown machine!\n");
    }
    // Allocate two byte for 16-bit accumulator in zeropage space
    SMA_allocateMemory(zeropageMem, 2);
}