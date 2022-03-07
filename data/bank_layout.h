//
// Bank Layout management
//
//  Keeps track of the size and location of the banks use for code and data
//
// Created by User on 3/5/2022.
//

#ifndef NEOLITHIC_BANK_LAYOUT_H
#define NEOLITHIC_BANK_LAYOUT_H

#include <stdbool.h>

#define MAX_BANKS 256      // 256 * 4k => 1 megabyte

struct BankDef {
    int memLoc;     // memory location of the start of the bank
    int fileLoc;    // where in the binary file the bank is located
    int size;       // size of the bank
};

struct BankLayout {
    bool usesMultipleBankSizes;
    int banksUsed;
    struct BankDef banks[MAX_BANKS];
};


extern void BL_init();
extern void BL_addBank(struct BankDef newBank);
extern void BL_done();
extern void BL_printBanks();
extern struct BankLayout *BL_getBankLayout();

#endif //NEOLITHIC_BANK_LAYOUT_H
