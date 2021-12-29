//
//  Expression evaluator
//
// Created by admin on 4/17/2020.
//

#include <stdlib.h>
#include <string.h>

#include "eval_expr.h"
#include "data/symbols.h"
#include "data/identifiers.h"


static SymbolTable *mainSymbolTable;
static SymbolTable *localSymbolTable;
static bool isEvalForAsm;

void initEvaluator(SymbolTable *symbolTable) {
    mainSymbolTable = symbolTable;
    localSymbolTable = NULL;
}

void setEvalLocalSymbolTable(SymbolTable *symbolTable) {
    localSymbolTable = symbolTable;
}

void setEvalExpressionMode(bool forASM) {
    isEvalForAsm = forASM;
}

SymbolRecord *getEvalSymbolRecord(const char *varName) {
    SymbolRecord *varSym = NULL;
    if (localSymbolTable != NULL) {
        varSym = findSymbol(localSymbolTable, varName);
    }
    if (varSym == NULL) {
        varSym = findSymbol(mainSymbolTable, varName);
    }
    return varSym;
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
            SymbolRecord *varSym = getEvalSymbolRecord(node.value.str);

            // when evaluating things for inline assembly code,
            //   symbols always return their location...
            //   unless they are simple consts (don't take RAM/ROM)
            if (varSym && isEvalForAsm) {
                if (HAS_SYMBOL_LOCATION(varSym)) {
                    result.hasResult = true;
                    result.value = varSym->location;
                } else {
                    result.hasResult = varSym->hasValue;
                    result.value = varSym->constValue;
                }
            } else if (varSym && isConst(varSym)) {
                result.hasResult = varSym->hasValue;
                result.value = varSym->constValue;
            } else {
                result.hasResult = false;
            }
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
        SymbolRecord *varSym = getEvalSymbolRecord(node.value.str);
        if (varSym) {
            result.value = varSym->location;
            result.hasResult = HAS_SYMBOL_LOCATION(varSym);
            //printf("eval_addr_of %s = %d\n", varSym->name, result.value);
        }
    }
    return result;
}

EvalResult evaluate_node(const ListNode node) {
    return eval_node(node);
}

EvalResult evaluate_enumeration(SymbolRecord *enumSymbol, ListNode propNode) {
    // only enumeration property values will return a value.
    EvalResult result;
    result.hasResult = false;

    SymbolRecord *propertySymbol = findSymbol(enumSymbol->symbolTbl, propNode.value.str);
    if (!propertySymbol) return result;      /// EXIT if invalid structure property

    // Handle ENUM references
    if (isEnum(enumSymbol)) {
        result.hasResult = true;
        result.value = propertySymbol->constValue;
    }

    return result;
}

EvalResult eval_property_ref(ListNode structNode, ListNode propNode) {
    // only enumeration property values will return a value.
    EvalResult result;
    result.hasResult = false;

    SymbolRecord *structSymbol = getEvalSymbolRecord(structNode.value.str);
    if (!structSymbol) return result;      /// EXIT if invalid structure
    if (structSymbol->symbolTbl == NULL) return result;

    return evaluate_enumeration(structSymbol, propNode);
}

EvalResult evaluate_expression(const List *expr) {
    EvalResult result, leftResult, rightResult;

    result.hasResult = false;
    if (expr->count < 2) return result;

    ListNode opNode = expr->nodes[0];
    enum ParseToken opToken = opNode.value.parseToken;

    // Handle special cases first
    switch (opToken) {
        case PT_ADDR_OF:
            leftResult = eval_addr_of(expr->nodes[1]);
            break;

        case PT_SIZEOF: {
            char *varName = expr->nodes[1].value.str;
            SymbolRecord *varSym = getEvalSymbolRecord(varName);
            if (varSym != NULL) {
                leftResult.value = calcVarSize(varSym);
                leftResult.hasResult = true;
                return leftResult;
            }
        } break;

        case PT_PROPERTY_REF:
            return eval_property_ref(expr->nodes[1], expr->nodes[2]);

        default:
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
                    case PT_LOOKUP:   result.value = leftResult.value + rightResult.value; break;
                    default:
                        result.hasResult = false;
                }
            }
        }
    } else if (leftResult.hasResult) {
        if (opNode.type == N_TOKEN) {
            result.hasResult = true;
            switch (opNode.value.parseToken) {
                case PT_NOT:        result.value = !leftResult.value; break;
                case PT_INVERT:     result.value = ~leftResult.value; break;
                case PT_LOW_BYTE:   result.value = (leftResult.value & 0xff); break;
                case PT_HIGH_BYTE:  result.value = ((leftResult.value >> 8) & 0xff); break;
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

/**
 * evalAsAddrLookup - Evaluate an expression as if it were an address lookup
 *
 * Used for 'alias' support.
 */
EvalResult evalAsAddrLookup(const List *expr) {
    EvalResult result, leftResult, rightResult;

    result.hasResult = false;
    if (expr->count < 2) return result;

    leftResult = eval_addr_of(expr->nodes[1]);
    rightResult = eval_node(expr->nodes[2]);

    SymbolRecord *varSym = getEvalSymbolRecord(expr->nodes[1].value.str);
    if ((varSym == NULL) || (!leftResult.hasResult) || (!rightResult.hasResult)) return result;

    // we have enough data to completely evaluate the expression
    int multiplier = getBaseVarSize(varSym);
    result.hasResult = true;
    result.value = leftResult.value + (rightResult.value * multiplier);
    return result;
}


//------------------------------------------------------------
//   Get expression string (for display in comments)


char* get_node(const ListNode node) {
    char *result;
    switch (node.type) {
        case N_INT:
            result = allocMem(8);
            sprintf(result, "%d", node.value.num);
            break;
        case N_LIST:
            return get_expression(node.value.list);
        case N_STR:
            if (isEvalForAsm) {
                char *varName = node.value.str;
                SymbolRecord *varSym = getEvalSymbolRecord(varName);
                if (varSym != NULL) {
                    result = (char *) getVarName(varSym);
                } else {
                    result = "ERROR";
                }
            } else {
                result = Ident_lookup(node.value.str);
            }
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

        result = allocMem(strlen(leftResult) + strlen(rightResult) + 5);
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
            case PT_PROPERTY_REF:
                strcat(result, ".");
                strcat(result, rightResult);
                break;
            default:
                result = (char *) EXPR_ERR;
        }
    } else {
        result = allocMem(strlen(leftResult) + 4);
        switch(opNode.value.parseToken) {
            case PT_NOT:
                strcpy(result, "!");
                strcat(result, leftResult);
                break;
            case PT_INVERT:
                strcpy(result, "~");
                strcat(result, leftResult);
                break;
            case PT_LOW_BYTE:
                strcpy(result, "<");
                strcat(result, leftResult);
                break;
            case PT_HIGH_BYTE:
                strcpy(result, ">");
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