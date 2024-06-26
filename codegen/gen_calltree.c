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
//  Neolithic Module: GCT - Generate Call Tree
//
//  Generate a call tree for:
//   - figuring out necessary stack space
//
// Created by admin on 10/17/2020.
//

#include <stdio.h>

#include "common/common.h"
#include "gen_calltree.h"
#include "data/func_map.h"

static SymbolTable *mainSymTable;


//-------------------------------------------------------------------------
//  Build a call tree to figure out function depths
//
//   {"SOURCE_FUNC", "DEST_FUNC"}

void GCT_FindFuncCalls(List *stmt, SymbolRecord *srcFunc) {
    // [if, expr, codeblock, codeblock]
    // [funcCall, name, params]

    ListNode opNode = stmt->nodes[0];
    if (isToken(opNode, PT_FUNC_CALL)) {
        char *destFuncName = stmt->nodes[1].value.str;
        SymbolRecord *destFuncSym = findSymbol(mainSymTable, destFuncName);
        if (destFuncSym) {
            FM_addCallToMap(srcFunc, destFuncSym);
        } else {
            // TODO: figure out better error handling here
            //        This warning can be caused by local function pointer variable usage.
            printf("WARNING: GCT Module - Unable to find function '%s'\n", destFuncName);
        }
    }
}

/**
 * Walk a tree of nodes, looking for function calls.
 * @param code
 * @param funcSym
 */
void GCT_WalkCodeNodes(List *code, SymbolRecord *funcSym) {
    GCT_FindFuncCalls(code, funcSym);

    if (code->hasNestedList) {

        // walk into any other nodes
        for_range(codeNodeNum, 1, code->count) {
            if (code->nodes[codeNodeNum].type == N_LIST)
                GCT_WalkCodeNodes(code->nodes[codeNodeNum].value.list, funcSym);
        }
    }
}


void GCT_CodeBlock(List *code, SymbolRecord *funcSym) {
    if (code->count < 1) return;     // Exit if empty function

    if (!isToken(code->nodes[0], PT_ASM)) {
        for_range(stmtNum, 1, code->count) {
            ListNode stmtNode = code->nodes[stmtNum];
            if (stmtNode.type == N_LIST) {
                GCT_WalkCodeNodes(stmtNode.value.list, funcSym);
            }
        }
    }
}


void GCT_Function(List *statement, int codeNodeIndex) {
    char *funcName = statement->nodes[1].value.str;
    if (statement->count >= codeNodeIndex) {
        ListNode codeNode = statement->nodes[codeNodeIndex];
        if (codeNode.type == N_LIST) {
            SymbolRecord *funcSym = findSymbol(mainSymTable, funcName);
            GCT_CodeBlock(codeNode.value.list, funcSym);
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
                    default: break;
                }
            }
        }
    }
}

bool hasWarnedAboutCallTreeDepth = false;
void generate_callTree(ListNode node, SymbolTable *symbolTable, bool isMain) {
    if (node.type != N_LIST) return;

    mainSymTable = symbolTable;

    List *program = node.value.list;
    if (program->nodes[0].value.parseToken != PT_PROGRAM) return;

    GCT_Program(program);

    int callTreeDepth = FM_calculateCallTree();
    if ((callTreeDepth > compilerOptions.maxFuncCallDepth) && !hasWarnedAboutCallTreeDepth) {
        printf("WARNING: Call tree is very deep: %d (cur limit: %d) \n",
               callTreeDepth,
               compilerOptions.maxFuncCallDepth);
        hasWarnedAboutCallTreeDepth = true;
    }

    if (compilerOptions.showCallTree && isMain) {
        FM_displayCallTree();
    }
}