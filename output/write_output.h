//
// Created by admin on 6/14/2020.
//

#ifndef MODULE_WRITE_OUTPUT_H
#define MODULE_WRITE_OUTPUT_H

#include <stdio.h>
#include "data/instr_list.h"
#include "output_block.h"

enum OutputType {OUT_DASM, OUT_BIN};

struct OutputAdapter {
    void (*init)(FILE *outFile);
    void (*done)();
    char* (*getExt)();

    void (*writeFunctionBlock)(const OutputBlock *block);
    void (*writeStaticArrayData)(const OutputBlock *block);
    void (*writeStaticStructData)(const OutputBlock *block);
};


extern struct OutputAdapter BIN_OutputAdapter;
extern struct OutputAdapter DASM_Adapter;

//-----------------------

extern void WO_Init(char *projectName, enum OutputType outputType);
extern void WO_Done();
extern void WO_StartOfBank();
extern void WO_EndOfBank();

extern void WO_PrintSymbolTable(SymbolTable *workingSymbolTable, char *symTableName);

extern void WO_WriteAllBlocks();


#endif //MODULE_WRITE_OUTPUT_H
