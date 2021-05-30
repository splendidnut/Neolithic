//
//  Neolithic Module: GCT - Generate Call Tree
//
//  Generate a call tree for:
//   - figuring out necessary stack space
//
// Created by admin on 10/17/2020.
//

#include <stdio.h>

#include "gen_calltree.h"
#include "data/func_map.h"

//-------------------------------------------------------------------------
//  TODO: Need to build a call tree to figure out function depths
//
//   {"SOURCE_FUNC", "DEST_FUNC"}

void GCT_FindFuncCalls(char* srcFuncName, List *stmt) {
    // [if, expr, codeblock, codeblock]
    // [funcCall, name, params]

    ListNode opNode = stmt->nodes[0];
    if (opNode.type == N_TOKEN && opNode.value.parseToken == PT_FUNC_CALL) {
        FM_addCallToMap(srcFuncName, stmt->nodes[1].value.str);
    }
}


void GCT_Statement(char *funcName, List *code) {

}


void GCT_CodeBlock(char *funcName, List *code) {
    if (code->count < 1) return;     // Exit if empty function

    if (code->nodes[0].value.parseToken != PT_ASM) {
        for (int stmtNum = 1; stmtNum < code->count; stmtNum++) {
            ListNode stmtNode = code->nodes[stmtNum];
            if (stmtNode.type == N_LIST) {
                GCT_Statement(funcName, stmtNode.value.list);
            }
        }
    }
}


void GCT_Function(List *statement, int codeNodeIndex) {
    char *funcName = statement->nodes[1].value.str;
    if (statement->count >= codeNodeIndex) {
        ListNode codeNode = statement->nodes[codeNodeIndex];
        if (codeNode.type == N_LIST) {
            printf("Processing function: %s\n", funcName);
            List *code = codeNode.value.list;
            GCT_CodeBlock(funcName, code);
        }
    }
}

void GCT_Program(List *list) {
    for (int stmtNum = 1;  stmtNum < list->count;  stmtNum++) {
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


void generate_callTree(ListNode node) {
    if (node.type != N_LIST) return;

    printf("Generating Call Tree...\n");
    List *program = node.value.list;
    if (program->nodes[0].value.parseToken == PT_PROGRAM) {
        GCT_Program(program);
    }
}