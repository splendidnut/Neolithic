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
// Created by admin on 5/5/2021.
//

#include <stdio.h>
#include <data/syntax_tree.h>
#include <data/symbols.h>
#include "gen_common.h"


SymbolTable *curFuncSymbolTable;


int GC_ErrorCount = 0;

//-------------------------------------------------------
//--- Error Handlers

void ErrorMessageWithList(const char* errorMsg, const List *stmt) {
    if (stmt != NULL) {
        printf("ERROR on line %d: %s ", stmt->lineNum, errorMsg);
        showList(stdout, stmt, 0);
    } else {
        printf("ERROR: %s", errorMsg);
    }
    printf("\n");
    GC_ErrorCount++;
}

void ErrorMessageWithNode(const char* errorMsg, ListNode node, int lineNum) {
    printf("ERROR on line %d: %s ", lineNum, errorMsg);
    showNode(stdout, node, 0);
    printf("\n");
    GC_ErrorCount++;
}

void ErrorMessage(const char* errorMsg, const char* errorStr, int lineNum) {
    printf("ERROR on line %d: %s", lineNum, errorMsg);
    if (errorStr != NULL) printf(" '%s'", errorStr);
    printf("\n");
    GC_ErrorCount++;
}

void WarningMessage(const char* warnMsg, const char* warnStr, int lineNum) {
    printf("WARNING on line %d: %s", lineNum, warnMsg);
    if (warnStr != NULL) printf(" '%s'", warnStr);
    printf("\n");
}

//-------------------------------------------------------
//--- Symbol lookup

SymbolRecord *lookupSymbolNode(const ListNode symbolNode, int lineNum) {
    char *symbolName = symbolNode.value.str;
    SymbolRecord *symRec = NULL;
    if (curFuncSymbolTable != NULL)
        symRec = findSymbol(curFuncSymbolTable, symbolName);
    if (symRec == NULL)
        symRec = findSymbol(mainSymbolTable, symbolName);
    if (symRec == NULL) {
        ErrorMessageWithNode("Symbol not found", symbolNode, lineNum);
    }
    return symRec;
}


SymbolRecord *lookupFunctionSymbolByNameNode(ListNode funcNameNode, int lineNum) {
    char *funcName = funcNameNode.value.str;
    SymbolRecord *funcSym = NULL;
    if (curFuncSymbolTable != NULL)
        funcSym = findSymbol(curFuncSymbolTable, funcName);
    if (funcSym == NULL)
        funcSym = findSymbol(mainSymbolTable, funcName);
    if (funcSym == NULL) {
        ErrorMessageWithNode("Function not defined", funcNameNode, lineNum);
    }
    return funcSym;
}


SymbolRecord* getArraySymbol(const List *expr, ListNode arrayNode) {
    SymbolRecord *arraySymbol = lookupSymbolNode(arrayNode, expr->lineNum);

    if ((arraySymbol != NULL) && (!isArray(arraySymbol) && !isPointer(arraySymbol))) {
        ErrorMessageWithNode("Not an array or pointer", arrayNode, expr->lineNum);
        arraySymbol = NULL;
    }
    return arraySymbol;
}

SymbolRecord *getPropertySymbol(List *expr) {
    ListNode structNameNode = expr->nodes[1];
    char *propName = expr->nodes[2].value.str;

    SymbolRecord *structSym = lookupSymbolNode(structNameNode, expr->lineNum);
    if (structSym == NULL || !isStructDefined(structSym)) return NULL;

    SymbolRecord *propertySym = findSymbol(getStructSymbolSet(structSym), propName);
    if (propertySym == NULL) {
        ErrorMessage("Property Symbol not found within struct", propName, expr->lineNum);
    }
    return propertySym;
}




/**
 * Check if provided node is a const value: numeric literal or const var
 */
bool isConstValueNode(ListNode node, int lineNum) {
    if (node.type == N_INT) return true;
    if (node.type != N_STR) return false;
    SymbolRecord *constSym = lookupSymbolNode(node, lineNum);
    return (constSym && isConst(constSym));
}

/**
 * Get const value from provided node: numeric literal or const var value
 */
int getConstValue(ListNode valueNode, int lineNum) {
    switch (valueNode.type) {
        case N_INT: return valueNode.value.num;
        case N_STR: {
            SymbolRecord *constSym = lookupSymbolNode(valueNode, lineNum);
            return constSym->constValue;
        }
        default: return 0;
    }
}