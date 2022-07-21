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
// Created by admin on 4/19/2020.
//

#ifndef MODULE_FUNC_MAP_H
#define MODULE_FUNC_MAP_H

//--------------------------------------------
//   Map of function calls:
//
//   {"SOURCE_FUNC", {"FUNC_CALLED2", "FUNC_CALLED2"} }

#include "symbols.h"

struct FuncCallMapEntryStruct;

typedef struct FuncCallMapEntryStruct {
    char *srcFuncName;
    SymbolRecord *funcSym;
    struct FuncCallMapEntryStruct *next;

    int cntFuncsCalled;         // number of function calls this function makes...
    int deepestSpotCalled;      // how deep in the stack will this function ever be called?
    char *dstFuncName[32];      // list of destinations (functions this function calls)
} FuncCallMapEntry;


extern FuncCallMapEntry* FM_findFunction(char *funcName);
extern void FM_addCallToMap(SymbolRecord *srcFuncSym, SymbolRecord *dstFuncSym);
extern void FM_displayCallTree();
extern void FM_addFunctionDef(SymbolRecord *funcSym);
extern int FM_calculateCallTree();

#endif //MODULE_FUNC_MAP_H
