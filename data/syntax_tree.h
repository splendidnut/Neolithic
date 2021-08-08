//
//   Syntax Tree module
//
//   List structures

#ifndef SYNTAX_TREE_MODULE
#define SYNTAX_TREE_MODULE

#include <stdbool.h>
#include <stdio.h>

#include "common/common.h"
#include "parser/tokens.h"
#include "cpu_arch/asm_code.h"

enum NodeType { N_EMPTY, N_INT, N_CHAR, N_STR, N_LIST, N_TOKEN, N_MNE, N_SYMBOL };

struct ListStruct;

typedef union {
    int num;
    char ch;
    char *str;
    struct ListStruct *list;
    enum ParseToken parseToken;
    enum MnemonicCode mne;
} NodeValue;  // can be int, bool, str

typedef struct {
    enum NodeType type;
    NodeValue value;
} ListNode;

typedef struct ListStruct {
    int count;
    int size;
    int lineNum;
    SourceCodeLine progLine;
    bool hasNestedList;
    ListNode nodes[];
} List;


//---  Node creation functions

extern ListNode createEmptyNode();
extern ListNode createIntNode(int num);
extern ListNode createCharNode(char ch);
extern ListNode createStrNode(const char *str);
extern ListNode createListNode(List *list);
extern ListNode createParseToken(enum ParseToken parseToken);

extern bool isListNode(ListNode node);
extern bool isToken(ListNode node, enum ParseToken parseToken);

//--- List functions

extern List * createList(int initialSize);
extern int getListCount(List *list);
extern int canAddToList(List *list);
extern int addNode(List *list, ListNode node);
extern List *wrapNode(ListNode node);
extern void showList(FILE *outputFile, const List *list, int indentLevel);
extern void destroyList(List *list);

#endif
