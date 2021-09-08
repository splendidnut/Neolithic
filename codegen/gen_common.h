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

extern SymbolRecord *lookupSymbolNode(ListNode symbolNode, int lineNum);

#endif //MODULE_GEN_COMMON_H
