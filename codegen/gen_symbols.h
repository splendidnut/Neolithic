//
// Created by admin on 3/27/2020.
//

#ifndef MODULE_GEN_SYMBOLS_H
#define MODULE_GEN_SYMBOLS_H

#include "data/syntax_tree.h"
#include "data/symbols.h"
#include "machine/mem.h"

extern void generate_symbols(ListNode node, SymbolTable *symbolTable, MemoryRange *varStorage);

#endif //MODULE_GEN_SYMBOLS_H
