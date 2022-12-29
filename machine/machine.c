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
// Neolithic target Machine module
//
// Provide support for targeting multiple 6502-based systems
//
// Created by User on 8/4/2021.
//

#include <stdio.h>
#include <string.h>
#include "mem.h"
#include "machine.h"

MachineInfo machineInfo[] = {
        {Machine_Unknown, "",0,0},
        {Atari2600, "Atari2600", 0x1000, 0x1FFF, 0x1FFF},
        {Atari5200, "Atari5200", 0x4000, 0xBFFF, 0xFFFF},
        {Atari7800, "Atari7800", 0x8000, 0xFFFF, 0xFFFF}
};

enum Machines lookupMachineName(char *machineName) {
    int index = 1;
    while (index < NUM_MACHINES) {
        if (strncmp(machineName, machineInfo[index].name, 12) == 0) break;
        index++;
    }
    if (index >= NUM_MACHINES) index = 0;
    return (enum Machines)index;
}

int getMachineStartAddr(enum Machines machine) {
    return machineInfo[machine].startAddr;
}

void prepForMachine(enum Machines machine) {
    MemoryArea *zeropageMem = 0;
    switch (machine) {
        case Atari2600:
            SMA_init(0);        // only has zeropage memory
            zeropageMem = SMA_addMemoryRange(0x80, 0xFF);
            break;

        case Atari5200:
            SMA_init(1);
            zeropageMem = SMA_addMemoryRange(0x00, 0xFF);
            SMA_addMemoryRange(0x0200, 0x3fff);
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
    if (zeropageMem) SMA_allocateMemory(zeropageMem, 2);
}

MachineInfo getMachineInfo(enum Machines machine) {
    return machineInfo[machine];
}