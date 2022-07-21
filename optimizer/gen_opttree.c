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
//  Generate Optimized Syntax Tree
//
//   This module is responsible for applying optimizations to the syntax tree before
//   code generation is run.
//
// Created by admin on 4/2/2021.
//

#include <data/syntax_tree.h>

#include "gen_opttree.h"
#include "common/tree_walker.h"


void WalkList(List *list, ProcessNodeFunc procNode) {
    for (int stmtNum = 1;  stmtNum < list->count;  stmtNum++) {
        ListNode stmt = list->nodes[stmtNum];
        if (stmt.type == N_LIST) {
            List *statement = stmt.value.list;
            ListNode opNode = statement->nodes[0];
            procNode(opNode, statement);
        } else {
            procNode(stmt, NULL);
        }
    }
}

//---------------------------------------------------------------------------------

void GOST_LoadPropertyRefOfs(const List *stmt, enum SymbolType destType) {}
void GOST_AddrOf(const List *stmt, enum SymbolType destType) {}
void GOST_Lookup(const List *stmt, enum SymbolType destType) {
    // Trying to optimize:
    //    (lookup, 'Maze', (add, 'mazeIndex', 1))

    ListNode arrayNameNode = stmt->nodes[1];
    ListNode indexNode = stmt->nodes[2];

    if (indexNode.type == N_LIST) {
        List *indexExpr = indexNode.value.list;
        if (indexExpr->nodes[0].value.parseToken == PT_ADD) {
            if ((indexExpr->nodes[1].type == N_SYMBOL) && (indexExpr->nodes[2].type == N_INT)) {

            }
        }
    }
}

//---------------------------------------------------------------------------------

ParseFuncTbl gostExprFunction[] = {
        {PT_PROPERTY_REF,   &GOST_LoadPropertyRefOfs},
        {PT_ADDR_OF,        &GOST_AddrOf},
        {PT_LOOKUP,         &GOST_Lookup},
/*        {PT_BIT_AND,        &GC_And},
        {PT_BIT_OR,         &GC_Or},
        {PT_BIT_EOR,        &GC_Eor},
        {PT_CAST,           &GC_Cast},
        {PT_INC,            &GC_Inc},
        {PT_DEC,            &GC_Dec},
        {PT_ADD,            &GC_Add},
        {PT_NEGATIVE,       &GC_Negate},
        {PT_SUB,            &GC_Sub},
        {PT_SHIFT_LEFT,     &GC_ShiftLeft},
        {PT_SHIFT_RIGHT,    &GC_ShiftRight},
        {PT_BOOL_AND,       &GC_BoolAnd},
        {PT_BOOL_OR,        &GC_BoolOr},
        {PT_NOT,            &GC_Not}*/
};
const int GOST_exprFuncSize = sizeof(gostExprFunction) / sizeof(ParseFuncTbl);


void GOST_Expression(List *expr, enum SymbolType destType) {
    ListNode opNode = expr->nodes[0];
    if (opNode.type == N_TOKEN) {
        callFunc(gostExprFunction, GOST_exprFuncSize, opNode.value.parseToken, expr, destType);
    }
}

//---------------------------------------------------------------------------------
// --- Forward declaration
void GOST_ProcessCodeBlock(List *code);

//--------------------------------------

void GOST_Assignment(const List *stmt, enum SymbolType destType) {

    // currently only care about right-side expression of assignment statement
    ListNode loadNode = stmt->nodes[2];
    if (loadNode.type == N_LIST) {
        GOST_Expression(loadNode.value.list, destType);
    }
}

void GOST_DoWhile(const List *stmt, enum SymbolType destType) {}
void GOST_While(const List *stmt, enum SymbolType destType) {}
void GOST_For(const List *stmt, enum SymbolType destType) {}
void GOST_If(const List *stmt, enum SymbolType destType) {}

void examineSwitchStmtCases(const List *switchStmt) {
    char caseValueSet[256];
    for (int i=0; i<256; i++) caseValueSet[i] = 0;

    // examine case statements... are they sequential?
    int caseCount = 0;
    for (int caseStmtNum = 2;  caseStmtNum < switchStmt->count;  caseStmtNum++) {
        ListNode caseStmtNode = switchStmt->nodes[caseStmtNum];
        if (caseStmtNode.type == N_LIST) {
            List *caseStmt = caseStmtNode.value.list;
            if (caseStmt->nodes[0].value.parseToken == PT_CASE) {
                ListNode caseValueNode = caseStmt->nodes[1];

                int caseValue;

                // check each node
                switch (caseValueNode.type) {
                    case N_INT: caseValue = caseValueNode.value.num; break;
                    case N_STR: {
                        /*SymbolRecord *symbol = lookupSymbolNode(caseValueNode, caseStmt->lineNum);
                        caseValue = symbol->constValue;
                        printf("Case found with symbol kind: %s\n", getNameOfSymbolKind(symbol->kind));*/
                    }break;
                    default:
                        caseValue = -1;
                        //ErrorMessageWithList("Invalid case value", caseStmt);
                        break;
                }

                if (caseValue >= 0) {
                    caseValueSet[caseValue] = caseStmtNum;
                    caseCount++;
                }
            }
        }
    }

    if (caseCount > 2) {

        int idx = 0;
        while ((caseValueSet[idx] == 0) && (idx < 256)) idx++;   // find first 1
        int startOffset = idx;
        while ((caseValueSet[idx] > 0) && (idx < 256)) idx++;    // move thru all sequential cases
        int endOffset = idx;

        if ((endOffset - startOffset) == caseCount) {
            // we can optimize
            printf("Can optimize (%d sequential cases): \n", caseCount);
            showList(stdout, switchStmt, 1);
            printf("\n\n");
            for (idx = startOffset; idx<endOffset; idx++) {
                printf("case #%d is %d\n", idx, caseValueSet[idx]);
            }
            printf("\n");
        }
    }
}


void GOST_Switch(const List *stmt, enum SymbolType destType) {

}


ParseFuncTbl GOST_stmtFunction[13] = {
        {PT_SET,        &GOST_Assignment},
        {PT_DOWHILE,    &GOST_DoWhile},
        {PT_WHILE,      &GOST_While},
        {PT_FOR,        &GOST_For},
        {PT_IF,         &GOST_If},
        {PT_SWITCH,     &GOST_Switch}
};
const int GOST_stmtFunctionSize = sizeof(GOST_stmtFunction) / sizeof(ParseFuncTbl);

void GOST_Statement(List *stmt) {
    int funcIndex;
    ListNode opNode = stmt->nodes[0];
    if (opNode.type == N_TOKEN) {

        // using statement token, lookup and call appropriate code generator function
        funcIndex = findFunc(GOST_stmtFunction, GOST_stmtFunctionSize, opNode.value.parseToken);
        if (funcIndex >= 0) {
            GOST_stmtFunction[funcIndex].parseFunc(stmt, ST_NONE);
        }
    }
}



void GOST_ProcessCodeBlock(List *code) {
    if (code->count < 1) return;     // Exit if empty function

    if (code->nodes[0].value.parseToken != PT_ASM) {
        for (int stmtNum = 1; stmtNum < code->count; stmtNum++) {
            ListNode stmtNode = code->nodes[stmtNum];
            if (stmtNode.type == N_LIST) {
                GOST_Statement(stmtNode.value.list);
            }
        }
    }
}


void GOST_Function(const List *function, int codeNodeIndex) {
    char *funcName = function->nodes[1].value.str;
    if (function->count >= codeNodeIndex) {
        ListNode codeNode = function->nodes[codeNodeIndex];
        if (codeNode.type == N_LIST) {
            printf("Processing optimizations for function: %s\n", funcName);
            GOST_ProcessCodeBlock(codeNode.value.list);
        }
    }
}

//---------------------------------------------------------------------------------

List* getProgramList(ListNode node) {
    if (node.type == N_LIST) {
        List *program = node.value.list;
        if (program->nodes[0].value.parseToken == PT_PROGRAM) {
            // generate all the code for the program
            return program;
        }
    }
    return NULL;
}

void GOST_ProcessProgramNodes(ListNode opNode, const List *statement) {
    if (opNode.type == N_TOKEN) {
        // Only need to process functions since we're doing code optimizations
        switch (opNode.value.parseToken) {
            case PT_DEFUN:    GOST_Function(statement, 5); break;
            case PT_FUNCTION: GOST_Function(statement, 3); break;
            default:
                break;
        }
    }
}


void optimize_parse_tree(ListNode node) {
    List* programList = getProgramList(node);
    if (programList != NULL) {
        WalkList(programList, &GOST_ProcessProgramNodes);
    }
}