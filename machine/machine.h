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
// Created by admin on 6/14/2021.
//

#ifndef MODULE_MACHINE_H
#define MODULE_MACHINE_H

#define NUM_MACHINES 4

enum Machines {
    Machine_Unknown,
    Atari2600,
    Atari5200,
    Atari7800
};

typedef struct {
    enum Machines machine;
    char* name;
    int startAddr;
    int endAddr;
    int addrMask;
} MachineInfo;

extern enum Machines lookupMachineName(char *machineName);
extern void prepForMachine(enum Machines machine);
extern MachineInfo getMachineInfo(enum Machines machine);

#endif //MODULE_MACHINE_H
