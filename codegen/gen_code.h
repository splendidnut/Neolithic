//
// Created by admin on 3/28/2020.
//

#ifndef MODULE_GEN_CODE_H
#define MODULE_GEN_CODE_H

#include "data/syntax_tree.h"
#include "data/symbols.h"

extern void initCodeGenerator(SymbolTable *symbolTable);
extern void generate_code(char *name, ListNode node);

#endif //MODULE_GEN_CODE_H
