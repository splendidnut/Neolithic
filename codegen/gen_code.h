//
// Created by admin on 3/28/2020.
//

#ifndef MODULE_GEN_CODE_H
#define MODULE_GEN_CODE_H

#include "data/syntax_tree.h"
#include "data/symbols.h"

extern void generate_code(ListNode node, SymbolTable *symbolTable, FILE *codeOutfile, bool isMainFile);

#endif //MODULE_GEN_CODE_H
