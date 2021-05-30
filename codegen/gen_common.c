//
// Created by admin on 5/5/2021.
//

#include <stdio.h>
#include <data/syntax_tree.h>
#include "gen_common.h"



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
