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
// Created by admin on 6/14/2020.
//

#ifndef MODULE_WRITE_OUTPUT_H
#define MODULE_WRITE_OUTPUT_H

#include <stdio.h>
#include "data/instr_list.h"
#include "data/bank_layout.h"
#include "output_block.h"

enum OutputType {OUT_DASM, OUT_BIN};

struct OutputAdapter {
    void (*init)(FILE *outFile, SymbolTable *mainSymTbl, struct BankLayout *bankLayout);
    void (*done)();
    char* (*getExt)();

    void (*writeFunctionBlock)(const OutputBlock *block);
    void (*writeStaticArrayData)(const OutputBlock *block);
    void (*writeStaticStructData)(const OutputBlock *block);

    void (*startWriteBlock)(const OutputBlock *block);
    void (*endWriteBlock)(const OutputBlock *block);
};


extern struct OutputAdapter BIN_OutputAdapter;
extern struct OutputAdapter DASM_Adapter;

//-----------------------

extern void WriteOutput(char *projectName, enum OutputType outputType, SymbolTable *mainSymTbl, struct BankLayout *bankLayout);


#endif //MODULE_WRITE_OUTPUT_H
