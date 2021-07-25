//
// Created by admin on 4/19/2020.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    FuncCallMapEntry *newFuncCallEntry = malloc(sizeof(FuncCallMapEntry));
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

void FM_addFuncCall(FuncCallMapEntry *mapEntry, char *destName) {
    if (destName == NULL) return;
    mapEntry->dstFuncName[mapEntry->cntFuncsCalled++] = destName;
}

void FM_addCallToMap(SymbolRecord *srcFuncSym, SymbolRecord *dstFuncSym) {
    FuncCallMapEntry *funcCallMapEntry = FM_findFunction(srcFuncSym->name);

    if (funcCallMapEntry == NULL) {
        FM_addNewFunc(srcFuncSym->name);
        funcCallMapEntry = curFuncCallEntry;
    }
    markFunctionUsed(dstFuncSym);
    FM_addFuncCall(funcCallMapEntry, dstFuncSym->name);
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

    // mark the depth of the current function
    funcMapEntry->deepestSpotCalled = depth-1;
    if (funcMapEntry->funcSym != NULL) {
        funcMapEntry->funcSym->funcExt->funcDepth = depth-1;
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

void FM_printNormalCallTree() {
    FuncCallMapEntry *funcMapEntry = firstFuncCallEntry;
    while (funcMapEntry != NULL) {
        SymbolRecord *funcSym = funcMapEntry->funcSym;
        printf(" * %s (depth %d, used %d):\n",
               funcMapEntry->srcFuncName,
               funcMapEntry->deepestSpotCalled,
               funcSym->funcExt->cntUses);

        int cntDestFunc;
        for (cntDestFunc = 0; cntDestFunc<funcMapEntry->cntFuncsCalled; cntDestFunc++) {
            printf("    -  %s\n", funcMapEntry->dstFuncName[cntDestFunc]);
        }

        funcMapEntry = funcMapEntry->next;
    }
}

int FM_calculateCallTree() {
    FuncCallMapEntry *funcMapEntry = FM_findFunction("main");
    if (funcMapEntry != NULL) {
        FM_followChainsToCalculateDepth(funcMapEntry, 1);
    }
    return deepestDepth;
}


void FM_displayCallTree() {
    printf("Function Call Tree\n");
    FM_printNormalCallTree();
}