//
// Created by User on 9/1/2021.
//

#ifndef NEOLITHIC_TYPE_CHECKER_H
#define NEOLITHIC_TYPE_CHECKER_H

#include <stdbool.h>

#include "data/syntax_tree.h"
#include "data/symbols.h"

bool TypeCheck_Alias(const List *expr, enum SymbolType destType);

#endif //NEOLITHIC_TYPE_CHECKER_H
