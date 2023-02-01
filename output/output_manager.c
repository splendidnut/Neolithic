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
// Manage the output of the compiler
//
// This module connects and manages all the modules involved in compiler output,
// providing a single point of tracking the output / memory layout of code.
//
// Created by User on 3/5/2022.
//

#include "output_manager.h"

#include <data/bank_layout.h>
#include <data/instr_list.h>
#include "output_block.h"
#include "write_output.h"

static MachineInfo outputTargetMachine;

void initOutputGenerator(enum Machines targetMachine) {
    // Configure output for specific machine
    // only need to initialize instruction list and output block modules once

    outputTargetMachine = getMachineInfo(targetMachine);

    BL_init();
    OB_Init();

    //-------------------------------------------------------------------------------------------
    // TODO: this is initial code to start incorporating code/data banks... need to flesh out
    // Build a default bank that matches the binary memory space available in the machine

    // Create main bank
    int bankSize = (outputTargetMachine.endAddr - outputTargetMachine.startAddr + 1);
    BL_addBank(bankSize, outputTargetMachine.startAddr, 0);

    if (targetMachine == Atari2600) {
        // TEMP (2600-only .. F8 bank-switching
        BL_addBank(bankSize, outputTargetMachine.startAddr + 0x2000, 0x1000);
    }

    BL_printBanks();
}

void generateOutput(char *projectName, SymbolTable *mainSymbolTable, OutputFlags outputFlags) {
    struct BankLayout *mainBankLayout = BL_getBankLayout();

    BL_printBanks();

    if (outputFlags.doOutputASM)
        WriteOutput(projectName, OUT_DASM, mainSymbolTable, mainBankLayout, outputTargetMachine);
    if (outputFlags.doOutputBIN)
        WriteOutput(projectName, OUT_BIN, mainSymbolTable, mainBankLayout, outputTargetMachine);
}