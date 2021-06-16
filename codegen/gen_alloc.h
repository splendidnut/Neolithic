//
// Created by admin on 6/14/2021.
//

#ifndef MODULE_GEN_ALLOC_H
#define MODULE_GEN_ALLOC_H

#include <data/syntax_tree.h>
#include <data/symbols.h>
#include <machine/mem.h>

extern void generate_var_allocations(SymbolTable *symbolTable, MemoryArea *varStorage);

#endif //MODULE_GEN_ALLOC_H
