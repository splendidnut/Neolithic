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
// Created by admin on 4/17/2020.
//

#ifndef MODULE_EVAL_EXPR_H
#define MODULE_EVAL_EXPR_H

#include <stdbool.h>
#include "data/symbols.h"
#include "data/syntax_tree.h"

typedef struct {
    bool hasResult;
    int value;
} EvalResult;

extern void initEvaluator(SymbolTable *symbolTable);
extern void setEvalLocalSymbolTable(SymbolTable *symbolTable);
extern void setEvalExpressionMode(bool forASM);
extern EvalResult evaluate_node(const ListNode node);
extern EvalResult evaluate_enumeration(SymbolRecord *enumSymbol, ListNode propNode);
extern EvalResult evaluate_expression(const List *expr);
extern EvalResult evalAsAddrLookup(const List *expr);
extern char* get_expression(const List *expr);

#endif //MODULE_EVAL_EXPR_H
