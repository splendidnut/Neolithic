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
// Bank Layout management
//
//  Keeps track of the size and location of the banks use for code and data
//
// Created by User on 3/5/2022.
//

#include <stdlib.h>
#include <stdio.h>
#include "common/common.h"
#include "bank_layout.h"

struct BankLayout *mainBankLayout;

void BL_init() {
    mainBankLayout = malloc(sizeof(struct BankLayout));
    mainBankLayout->banksUsed = 0;
    mainBankLayout->usesMultipleBankSizes = false;
}

void BL_addBank(int size, int memLoc, int fileLoc) {
    struct BankDef newBank;
    newBank.size = size;
    newBank.memLoc = memLoc;
    newBank.fileLoc = fileLoc;

    mainBankLayout->banks[mainBankLayout->banksUsed] = newBank;
    mainBankLayout->banksUsed++;
}

int BL_getMachineAddr(int bankNum) {
    return mainBankLayout->banks[bankNum].memLoc;
}

void BL_done() {
    if (mainBankLayout) free(mainBankLayout);
}

void BL_printBanks() {
    printf("\nBank layout:\n");
    printf("------------------\n");
    for_range(curBank, 0, mainBankLayout->banksUsed) {
        struct BankDef curBankDef = mainBankLayout->banks[curBank];
        int bankEnd = (curBankDef.memLoc + curBankDef.size - 1);

        printf("  %d: %4X - %4X\n", curBank, curBankDef.memLoc, bankEnd);
    }
    printf("\n");
}

BankLayout *BL_getBankLayout() {
    return mainBankLayout;
}