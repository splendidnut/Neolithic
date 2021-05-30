//
// Created by admin on 4/19/2020.
//

#ifndef MODULE_FUNC_MAP_H
#define MODULE_FUNC_MAP_H

//--------------------------------------------
//   Map of function calls:
//
//   {"SOURCE_FUNC", {"FUNC_CALLED2", "FUNC_CALLED2"} }

struct FuncCallMapEntryStruct;

typedef struct FuncCallMapEntryStruct {
    char *srcFuncName;
    struct FuncCallMapEntryStruct *next;

    int cntFuncsCalled;
    char *dstFuncName[30];
} FuncCallMapEntry;



extern void FM_addCallToMap(char *srcName, char *destName);
extern void FM_displayCallTree();

#endif //MODULE_FUNC_MAP_H
