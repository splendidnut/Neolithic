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

enum Machines lookupMachineName(char *machineName);
void prepForMachine(enum Machines machine);

#endif //MODULE_MACHINE_H
