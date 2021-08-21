//
// Created by admin on 5/5/2021.
//

#include <stdio.h>
#include <data/syntax_tree.h>
#include <data/symbols.h>
#include "gen_common.h"

SymbolTable *mainSymbolTable;
SymbolTable *curFuncSymbolTable;
SymbolTable *curFuncParamTable;


int GC_ErrorCount = 0;

//-------------------------------------------------------
//--- Error Handlers

void ErrorMessageWithList(const char* errorMsg, const List *stmt) {
    printf("ERROR on line %d: %s ", stmt->lineNum, errorMsg);
    if (stmt != NULL) {
        showList(stdout, stmt, 0);
    }
    printf("\n");
    GC_ErrorCount++;
}

void ErrorMessageWithNode(const char* errorMsg, ListNode node, int lineNum) {
    printf("ERROR on line %d: %s ", lineNum, errorMsg);
    showList(stdout, wrapNode(node), 0);
    printf("\n");
    GC_ErrorCount++;
}

void ErrorMessage(const char* errorMsg, const char* errorStr, int lineNum) {
    printf("ERROR on line %d: %s", lineNum, errorMsg);
    if (errorStr != NULL) printf(" %s", errorStr);
    printf("\n");
    GC_ErrorCount++;
}

//-------------------------------------------------------
//--- Symbol lookup

SymbolRecord *lookupSymbolNode(const ListNode symbolNode, int lineNum) {
    char *symbolName = symbolNode.value.str;
    SymbolRecord *symRec = NULL;
    if (curFuncSymbolTable != NULL)
        symRec = findSymbol(curFuncSymbolTable, symbolName);
    if ((symRec == NULL) && (curFuncParamTable != NULL))
        symRec = findSymbol(curFuncParamTable, symbolName);
    if (symRec == NULL)
        symRec = findSymbol(mainSymbolTable, symbolName);
    if (symRec == NULL) {
        ErrorMessageWithNode("Symbol not found", symbolNode, lineNum);
    }
    return symRec;
}
