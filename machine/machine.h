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

extern enum Machines lookupMachineName(char *machineName);
extern int getMachineStartAddr(enum Machines machine);
extern void prepForMachine(enum Machines machine);

#endif //MODULE_MACHINE_H
