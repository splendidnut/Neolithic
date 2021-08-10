//
// Created by admin on 4/17/2020.
//

#ifndef MODULE_EVAL_EXPR_H
#define MODULE_EVAL_EXPR_H

#include <stdbool.h>
#include "data/syntax_tree.h"

typedef struct {
    bool hasResult;
    int value;
} EvalResult;

extern void initEvaluator(SymbolTable *symbolTable);
extern void setEvalLocalSymbolTable(SymbolTable *symbolTable);
extern void setEvalExpressionMode(bool forASM);
extern EvalResult evaluate_expression(const List *expr);
extern char* get_expression(const List *expr);

#endif //MODULE_EVAL_EXPR_H
