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

    int cntFuncsCalled;         // number of function calls this function makes...
    int deepestSpotCalled;      // how deep in the stack will this function ever be called?
    char *dstFuncName[32];      // list of destinations (functions this function calls)
} FuncCallMapEntry;



extern void FM_addCallToMap(char *srcName, char *destName);
extern void FM_displayCallTree();
extern void FM_addFunctionDef(char *name);
extern int FM_calculateCallTree();

#endif //MODULE_FUNC_MAP_H
