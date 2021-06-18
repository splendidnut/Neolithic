//
//  Neolithic Module: GCT - Generate Call Tree
//
//  Generate a call tree for:
//   - figuring out necessary stack space
//
// Created by admin on 10/17/2020.
//

#include <stdio.h>
#include <string.h>

#include "common/common.h"
#include "gen_calltree.h"
#include "data/func_map.h"

static SymbolTable *mainSymTable;

#define DEBUG_CALL_TREE

//-------------------------------------------------------------------------
//  TODO: Need to build a call tree to figure out function depths
//
//   {"SOURCE_FUNC", "DEST_FUNC"}

void GCT_FindFuncCalls(char* srcFuncName, List *stmt) {
    // [if, expr, codeblock, codeblock]
    // [funcCall, name, params]

    ListNode opNode = stmt->nodes[0];
    if (isToken(opNode, PT_FUNC_CALL)) {
        char *destFuncName = strdup(stmt->nodes[1].value.str);
#ifdef DEBUG_CALL_TREE
        printf("\tFound Call: %s\n", destFuncName);
#endif
        FM_addCallToMap(srcFuncName, destFuncName);
    }
}

void GCT_WalkCodeNodes(char *funcName, List *code) {
    GCT_FindFuncCalls(funcName, code);

    if (code->hasNestedList) {

        // walk into any other nodes
        //for (int codeNodeNum = 1; codeNodeNum < code->count; codeNodeNum++) {
        for_range(codeNodeNum, 1, code->count) {
            if (code->nodes[codeNodeNum].type == N_LIST)
                GCT_WalkCodeNodes(funcName, code->nodes[codeNodeNum].value.list);
        }
    }
}


void GCT_CodeBlock(char *funcName, List *code) {
    if (code->count < 1) return;     // Exit if empty function

    if (!isToken(code->nodes[0], PT_ASM)) {
        for_range(stmtNum, 1, code->count) {
            ListNode stmtNode = code->nodes[stmtNum];
            if (stmtNode.type == N_LIST) {
                GCT_WalkCodeNodes(funcName, stmtNode.value.list);
            }
        }
    }
}


void GCT_Function(List *statement, int codeNodeIndex) {
    char *funcName = statement->nodes[1].value.str;
    if (statement->count >= codeNodeIndex) {
        ListNode codeNode = statement->nodes[codeNodeIndex];
        if (codeNode.type == N_LIST) {
#ifdef DEBUG_CALL_TREE
            printf("Processing function: %s\n", funcName);
#endif
            SymbolRecord *funcSym = findSymbol(mainSymTable, funcName);
            List *code = codeNode.value.list;
            FM_addFunctionDef(funcName, funcSym);
            GCT_CodeBlock(funcName, code);
        }
    }
}

void GCT_Program(List *list) {
    for_range(stmtNum, 1, list->count) {
        ListNode stmt = list->nodes[stmtNum];
        if (stmt.type == N_LIST) {
            List *statement = stmt.value.list;
            if (statement->nodes[0].type == N_TOKEN) {
                enum ParseToken token = statement->nodes[0].value.parseToken;
                switch (token) {
                    case PT_DEFUN:    GCT_Function(statement, 5); break;
                    case PT_FUNCTION: GCT_Function(statement, 3); break;
                }
            }
        }
    }
}


void generate_callTree(ListNode node, SymbolTable *symbolTable) {
    if (node.type != N_LIST) return;

    printf("Generating Call Tree...\n");
    mainSymTable = symbolTable;

    List *program = node.value.list;
    if (program->nodes[0].value.parseToken == PT_PROGRAM) {
        GCT_Program(program);
        int callTreeDepth = FM_calculateCallTree();
        if (callTreeDepth > 3) {
            printf("WARNING: Call tree is very Deep\n");
        }
#ifdef DEBUG_CALL_TREE
        FM_displayCallTree();
#endif
    }
}