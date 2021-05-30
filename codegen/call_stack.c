//
//  Neolithic - Code Generator - Call Stack
//
//  Keep track of source code call stack to make sure it doesn't blow out stack space
//
// Created by admin on 3/12/2021.
//

#include <stdbool.h>

#include <data/syntax_tree.h>
#include "call_stack.h"

List* callStack[32];
int callStackLen;
int callStackLimit;

void InitCallStack(int maxStackSize) {
    callStackLen = 0;
    callStackLimit = maxStackSize < 32 ? maxStackSize : 32;
}

bool CallStack_Add(List *callStmt) {
    if (callStackLen < callStackLimit) {
        callStack[callStackLen++] = callStmt;
        return true;
    } else {
        return false;
    }
}

List* CallStack_Pop() {
    if (callStackLen > 0) {
        callStackLen--;
        return callStack[callStackLen];
    } else {
        return NULL;
    }
}