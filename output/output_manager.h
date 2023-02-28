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
extern void addBankToOutputGenerator();
extern void generateOutput(char *projectName, SymbolTable *mainSymbolTable, OutputFlags outputFlags);

#endif //NEOLITHIC_OUTPUT_MANAGER_H
