//
// Created by admin on 4/2/2021.
//

#include "tree_walker.h"

//------------------------------------------------------

int findFunc(ParseFuncTbl function[], int tblSize, enum ParseToken token) {
    for (int index=0; index < tblSize; index++) {
        if (function[index].parseToken == token) return index;
    }
    return -1;
}

void callFunc(ParseFuncTbl functionList[], int tblSize, enum ParseToken token,
        List *dataList, enum SymbolType destType) {
    int funcIndex = findFunc(functionList, tblSize, token);
    if (funcIndex >= 0) {
        functionList[funcIndex].parseFunc(dataList, destType);
    }
}