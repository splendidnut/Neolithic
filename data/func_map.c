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
// Created by admin on 4/19/2020.
//

#include <stdio.h>
#include <string.h>

#include "func_map.h"

static FuncCallMapEntry *firstFuncCallEntry;
static FuncCallMapEntry *curFuncCallEntry;


FuncCallMapEntry* FM_findFunction(char *funcName) {
    FuncCallMapEntry *funcMapEntry = firstFuncCallEntry;
    while ((funcMapEntry != NULL) && (strcmp(funcMapEntry->srcFuncName, funcName))!=0) {
        funcMapEntry = funcMapEntry->next;
    }
    return funcMapEntry;
}

FuncCallMapEntry * FM_addNewFunc(char *srcName) {
    FuncCallMapEntry *newFuncCallEntry = allocMem(sizeof(FuncCallMapEntry));
    newFuncCallEntry->srcFuncName = srcName;
    newFuncCallEntry->deepestSpotCalled = -1;
    newFuncCallEntry->cntFuncsCalled = 0;
    newFuncCallEntry->next = NULL;

    if (firstFuncCallEntry == NULL) {
        firstFuncCallEntry = newFuncCallEntry;
        firstFuncCallEntry->next = NULL;
    } else {
        curFuncCallEntry->next = newFuncCallEntry;
    }
    curFuncCallEntry = newFuncCallEntry;

    return newFuncCallEntry;
}

void FM_addCallToMap(SymbolRecord *srcFuncSym, SymbolRecord *dstFuncSym) {
    FuncCallMapEntry *funcCallMapEntry = FM_findFunction(srcFuncSym->name);

    if (funcCallMapEntry == NULL) {
        FM_addNewFunc(srcFuncSym->name);
        funcCallMapEntry = curFuncCallEntry;
    }
    markFunctionUsed(dstFuncSym);
    if (dstFuncSym->name != NULL) {

        // check if function already in list
        int idx = 0;
        int cnt = funcCallMapEntry->cntFuncsCalled;
        while (idx < cnt) {
            if (strcmp(funcCallMapEntry->dstFuncName[idx], dstFuncSym->name)==0) {
                funcCallMapEntry->dstFuncCallCnt[idx]++;
                break;
            }
            idx++;
        }

        // if we didn't find it, add it
        if (idx == cnt) {
            funcCallMapEntry->dstFuncCallCnt[cnt] = 1;
            funcCallMapEntry->dstFuncName[cnt] = dstFuncSym->name;
            funcCallMapEntry->cntFuncsCalled++;
        }
    }
}

void FM_addFunctionDef(SymbolRecord *funcSym) {
    FuncCallMapEntry *funcCallMapEntry = FM_findFunction(funcSym->name);
    if (funcCallMapEntry == NULL) {
        funcCallMapEntry = FM_addNewFunc(funcSym->name);
        funcCallMapEntry->funcSym = funcSym;
    }
}

//=====================================================================

int deepestDepth = 0;
void FM_followChainsToCalculateDepth(FuncCallMapEntry *funcMapEntry, int depth) {

    // mark the depth of the current function (only if it's deeper than before
    int newDepth = depth-1;

    if (funcMapEntry->deepestSpotCalled < newDepth) {
        funcMapEntry->deepestSpotCalled = newDepth;

        if (funcMapEntry->funcSym != NULL) {
            funcMapEntry->funcSym->funcDepth = newDepth;
        }
    }

    int cntDestFunc = 0;
    while (cntDestFunc<funcMapEntry->cntFuncsCalled) {
        if (depth > deepestDepth) deepestDepth = depth;

        char *dstName = funcMapEntry->dstFuncName[cntDestFunc];
        FuncCallMapEntry *nextChainNode = FM_findFunction(dstName);
        if (nextChainNode != NULL) {
            FM_followChainsToCalculateDepth(nextChainNode, depth + 1);
        }
        cntDestFunc++;
    }
}

int FM_calculateCallTree() {
    FuncCallMapEntry *funcMapEntry = FM_findFunction("main");
    if (funcMapEntry != NULL) {
        FM_followChainsToCalculateDepth(funcMapEntry, 1);
    }
    return deepestDepth;
}

//--------------------------------------------------------------------------
//  Functions for displaying information about the Function Map / Call Tree

void FM_printChainNode(FuncCallMapEntry *funcMapEntry, int depth) {
    int cntDestFunc;
    for (cntDestFunc = 0; cntDestFunc<funcMapEntry->cntFuncsCalled; cntDestFunc++) {
        // print start of chain
        for (int d = 0; d < depth; d++) {
            printf("  ");
        }
        printf("%s ->", funcMapEntry->srcFuncName);

        char *dstName = funcMapEntry->dstFuncName[cntDestFunc];
        FuncCallMapEntry *nextChainNode = FM_findFunction(dstName);

        if (nextChainNode != NULL) {
            FM_printChainNode(nextChainNode, depth+1);
        } else {
            //print last node:
            printf("%s\n", dstName);
        }
    }
}

void FM_displayCallTree() {
    printf("\nFunction Call Tree\n");
    FuncCallMapEntry *funcMapEntry = firstFuncCallEntry;
    while (funcMapEntry != NULL) {
        SymbolRecord *funcSym = funcMapEntry->funcSym;
        printf(" * %s (depth %d, used %d):\n",
               funcMapEntry->srcFuncName,
               funcMapEntry->deepestSpotCalled,
               funcSym->cntUses);

        int cntDestFunc;
        for (cntDestFunc = 0; cntDestFunc < funcMapEntry->cntFuncsCalled; cntDestFunc++) {
            printf("    - %2dx - %s\n",
                   funcMapEntry->dstFuncCallCnt[cntDestFunc],
                   funcMapEntry->dstFuncName[cntDestFunc]);
        }

        funcMapEntry = funcMapEntry->next;
    }
}