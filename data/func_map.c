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

void FM_addNewFunc(char *srcName) {
    FuncCallMapEntry *newFuncCallEntry = malloc(sizeof(FuncCallMapEntry));
    newFuncCallEntry->srcFuncName = srcName;
    newFuncCallEntry->cntFuncsCalled = 0;

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

void FM_displayCallTree() {
    printf("Function Call\n");
    FuncCallMapEntry *funcMapEntry = firstFuncCallEntry;
    while (funcMapEntry != NULL) {
        printf(" * %s:\n", funcMapEntry->srcFuncName);

        int cntDestFunc;
        for (cntDestFunc = 0; cntDestFunc<funcMapEntry->cntFuncsCalled; cntDestFunc++) {
            printf("    -  %s\n", funcMapEntry->dstFuncName[cntDestFunc]);
        }

        funcMapEntry = funcMapEntry->next;
    }
}