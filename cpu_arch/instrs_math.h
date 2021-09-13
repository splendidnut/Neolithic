//
// Created by pblackman on 5/11/2021.
//

#ifndef MODULE_INSTRS_MATH_H
#define MODULE_INSTRS_MATH_H

#include <data/instr_list.h>

extern void ICG_Mul_InitLookupTables(SymbolTable *globalSymbolTable);
extern void ICG_Mul_AddLookupTable(char lookupValue);
extern void ICG_MultiplyWithVar(const SymbolRecord *varRec, const SymbolRecord *varRec2);
extern void ICG_MultiplyWithConst(const SymbolRecord *varRec, const char multiplier);

#endif //MODULE_INSTRS_MATH_H
