//
// Created by admin on 5/5/2021.
//

#ifndef MODULE_GEN_COMMON_H
#define MODULE_GEN_COMMON_H

extern int GC_ErrorCount;

extern void ErrorMessageWithList(const char* errorMsg, const List *stmt);
extern void ErrorMessageWithNode(const char* errorMsg, ListNode node, int lineNum);
extern void ErrorMessage(const char* errorMsg, const char* errorStr, int lineNum);

#endif //MODULE_GEN_COMMON_H
