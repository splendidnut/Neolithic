//
// Created by admin on 6/14/2020.
//

#ifndef MODULE_WRITE_OUTPUT_H
#define MODULE_WRITE_OUTPUT_H

#include "data/instr_list.h"

extern void WO_Init(FILE *outFile);
extern void WO_Done();
extern void WO_StartOfBank();
extern void WO_EndOfBank();
extern void WO_Variable(const char *name, const char *value);
extern void WO_FuncSymTables(SymbolRecord *funcSym);

extern void WO_PrintSymbolTable(SymbolTable *workingSymbolTable, char *symTableName);

extern void WO_WriteAllBlocks();


#endif //MODULE_WRITE_OUTPUT_H
