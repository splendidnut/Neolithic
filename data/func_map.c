//
// Created by admin on 4/19/2020.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "func_map.h"
#include "symbols.h"

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
}

void FM_addFuncCall(FuncCallMapEntry *mapEntry, char *destName) {
    mapEntry->dstFuncName[mapEntry->cntFuncsCalled++] = destName;
}

void FM_addCallToMap(char *srcName, char *destName) {
    FuncCallMapEntry *funcCallMapEntry = FM_findFunction(srcName);

    if (funcCallMapEntry == NULL) {
        FM_addNewFunc(srcName);
        funcCallMapEntry = curFuncCallEntry;
    }
    FM_addFuncCall(funcCallMapEntry, destName);
}

void FM_addFunctionDef(char *name, SymbolRecord *funcSym) {
    FuncCallMapEntry *funcCallMapEntry = FM_findFunction(name);
    if (funcCallMapEntry == NULL) {
        funcCallMapEntry = FM_addNewFunc(name);
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
        printf(" * %s (depth %d):\n", funcMapEntry->srcFuncName, funcMapEntry->deepestSpotCalled);

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