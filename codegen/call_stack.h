//
//  Neolithic - Code Generator - Call Stack
//
//  Keep track of source code call stack to make sure it doesn't blow out stack space
//
// Created by admin on 3/12/2021.
//

#ifndef MODULE_CALL_STACK_H
#define MODULE_CALL_STACK_H

extern void InitCallStack(int maxStackSize);
extern bool CallStack_Add(List *callStmt);
extern List* CallStack_Pop();

#endif //MODULE_CALL_STACK_H
