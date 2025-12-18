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
// Created by admin on 5/5/2021.
//

#ifndef MODULE_GEN_COMMON_H
#define MODULE_GEN_COMMON_H

#include "data/symbols.h"
#include "data/syntax_tree.h"

extern SymbolTable *mainSymbolTable;
extern SymbolTable *curFuncSymbolTable;

extern int GC_ErrorCount;

extern void ErrorMessageWithList(const char* errorMsg, const List *stmt);
extern void ErrorMessageWithNode(const char* errorMsg, ListNode node, int lineNum);
extern void ErrorMessage(const char* errorMsg, const char* errorStr, int lineNum);
extern void WarningMessage(const char* warnMsg, const char* warnStr, int lineNum);

extern SymbolRecord *lookupSymbolNode(ListNode symbolNode, int lineNum);
extern SymbolRecord *lookupFunctionSymbolByNameNode(ListNode funcNameNode, int lineNum);
extern SymbolRecord* getArraySymbol(const List *expr, ListNode arrayNode);
extern SymbolRecord *getPropertySymbol(List *expr);

extern bool isConstValueNode(ListNode node, int lineNum);
extern int getConstValue(ListNode valueNode, int lineNum);

#endif //MODULE_GEN_COMMON_H
