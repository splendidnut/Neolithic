//
// Created by User on 3/5/2022.
//

#ifndef NEOLITHIC_OUTPUT_MANAGER_H
#define NEOLITHIC_OUTPUT_MANAGER_H

#include <machine/machine.h>
#include <stdbool.h>
#include <data/symbols.h>

typedef struct {
    bool doOutputASM;
    bool doOutputBIN;
} OutputFlags;

extern void initOutputGenerator(enum Machines targetMachine);
extern void generateOutput(char *projectName, SymbolTable *mainSymbolTable, OutputFlags outputFlags);

#endif //NEOLITHIC_OUTPUT_MANAGER_H
