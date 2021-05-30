//
//  Expression evaluator
//
// Created by admin on 4/17/2020.
//

#include <stdlib.h>
#include <string.h>
#include "data/symbols.h"
#include "eval_expr.h"

static SymbolTable *mainSymbolTable;

void initEvaluator(SymbolTable *symbolTable) {
    mainSymbolTable = symbolTable;
}

EvalResult eval_node(ListNode node) {
    EvalResult result;
    switch (node.type) {
        case N_INT:
            result.hasResult = true;
            result.value = node.value.num;
            break;
        case N_LIST:
            return evaluate_expression(node.value.list);
        case N_STR: {
            SymbolRecord *varSym = findSymbol(mainSymbolTable, node.value.str);
            if (varSym && isConst(varSym)) {
                result.hasResult = varSym->hasValue;
                result.value = varSym->constValue;
            } else result.hasResult = false;
        } break;
        default:
            result.hasResult = false;
    }
    return result;
}

EvalResult eval_addr_of(ListNode node) {
    EvalResult result;
    result.hasResult = false;
    if (node.type == N_STR) {
        SymbolRecord *varSym = findSymbol(mainSymbolTable, node.value.str);
        if (varSym) {
            result.value = varSym->location;
            result.hasResult = varSym->hasLocation;
            //printf("eval_addr_of %s = %d\n", varSym->name, result.value);
        }
    }
    return result;
}

EvalResult evaluate_expression(const List *expr) {
    EvalResult result, leftResult, rightResult;

    result.hasResult = false;
    if (expr->count < 2) return result;

    ListNode opNode = expr->nodes[0];
    enum ParseToken opToken = opNode.value.parseToken;

    if (opToken == PT_ADDR_OF) {
        leftResult = eval_addr_of(expr->nodes[1]);
    } else {
        leftResult = eval_node(expr->nodes[1]);
    }

    if (expr->count >= 3) {
        rightResult = eval_node(expr->nodes[2]);

        if (leftResult.hasResult && rightResult.hasResult) {
            if (opNode.type == N_TOKEN) {
                result.hasResult = true;
                switch (opNode.value.parseToken) {
                    case PT_ADD:      result.value = leftResult.value + rightResult.value; break;
                    case PT_SUB:      result.value = leftResult.value - rightResult.value; break;
                    case PT_MULTIPLY: result.value = leftResult.value * rightResult.value; break;
                    case PT_DIVIDE:   result.value = leftResult.value / rightResult.value; break;
                    case PT_BIT_AND:  result.value = leftResult.value & rightResult.value; break;
                    case PT_BIT_OR:   result.value = leftResult.value | rightResult.value; break;
                    case PT_BIT_EOR:  result.value = leftResult.value ^ rightResult.value; break;
                    case PT_LOOKUP: result.value = leftResult.value + rightResult.value; break;
                    default:
                        result.hasResult = false;
                }
            }
        }
    } else if (leftResult.hasResult) {
        if (opNode.type == N_TOKEN) {
            result.hasResult = true;
            switch (opNode.value.parseToken) {
                case PT_NOT:    result.value = !leftResult.value; break;
                case PT_INVERT: result.value = ~leftResult.value; break;
                case PT_ADDR_OF:
                    result.value = leftResult.value;
                    result.hasResult = leftResult.hasResult;
                    break;
                default:
                    result.hasResult = false;
            }
        }
    }

    /*
    if (result.hasResult) {
        printf("After op %s, result is: %d\n", getParseTokenName(opToken), result.value);
    }*/

    return result;
}


//------------------------------------------------------------
//   Get expression string (for display in comments)

char* get_node(const ListNode node) {
    char *result;
    switch (node.type) {
        case N_INT:
            result = malloc(8);
            sprintf(result, "%d", node.value.num);
            break;
        case N_LIST:
            return get_expression(node.value.list);
        case N_STR:
            result = strdup(node.value.str);
            break;
        default:
            result = 0;
    }
    return result;
}

const char *EXPR_ERR = "#ERROR#";
char* get_expression(const List *expr) {
    char *result;
    ListNode opNode = expr->nodes[0];
    char *leftResult = get_node(expr->nodes[1]);

    if (expr->count >= 3) {
        char *rightResult = get_node(expr->nodes[2]);

        if (!(leftResult && rightResult) || (opNode.type != N_TOKEN)) return (char *) EXPR_ERR;

        result = malloc(strlen(leftResult) + strlen(rightResult) + 5);
        strcpy(result, leftResult);
        switch (opNode.value.parseToken) {
            case PT_ADD:
                strcat(result, " + ");
                strcat(result, rightResult);
                break;
            case PT_SUB:
                strcat(result, " - ");
                strcat(result, rightResult);
                break;
            case PT_MULTIPLY:
                strcat(result, " * ");
                strcat(result, rightResult);
                break;
            case PT_DIVIDE:
                strcat(result, " / ");
                strcat(result, rightResult);
                break;
            case PT_BIT_OR:
                strcat(result, " | ");
                strcat(result, rightResult);
                break;
            case PT_BIT_AND:
                strcat(result, " & ");
                strcat(result, rightResult);
                break;
            case PT_LOOKUP:
                strcat(result, "[");
                strcat(result, rightResult);
                strcat(result, "]");
                break;
            default:
                result = (char *) EXPR_ERR;
        }
    } else {
        result = malloc(strlen(leftResult) + 4);
        switch(opNode.value.parseToken) {
            case PT_NOT:
                strcpy(result, "!");
                strcat(result, leftResult);
                break;
            case PT_INVERT:
                strcpy(result, "~");
                strcat(result, leftResult);
                break;
            case PT_ADDR_OF:
                strcpy(result, "&");
                strcat(result, leftResult);
                break;
            default:
                result = (char *) EXPR_ERR;
        }
    }
    return result;
}