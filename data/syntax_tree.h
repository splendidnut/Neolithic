/***************************************************************************
 * Neolithic Compiler - Simple C Cross-compiler for the 6502
 *
 * Copyright (c) 2020-2022 by Philip Blackman
 * -------------------------------------------------------------------------
 *
 * Licensed under the GNU General Public License v2.0
 *
 * See the "LICENSE.TXT" file for more information regarding usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * -------------------------------------------------------------------------
 */

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

enum NodeType { N_EMPTY, N_INT, N_CHAR, N_STR, N_LIST, N_TOKEN, N_MNE, N_ADDR_MODE, N_SYMBOL, N_STR_LITERAL };

struct ListStruct;

typedef union {
    int num;
    char ch;
    char *str;
    struct ListStruct *list;
    enum ParseToken parseToken;
    enum MnemonicCode mne;
    enum AddrModes addrMode;
} NodeValue;  // can be int, bool, str

typedef struct ListNode {
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
extern ListNode createStrLiteralNode(const char *str);
extern ListNode createListNode(List *list);
extern ListNode createParseToken(enum ParseToken parseToken);
extern ListNode createMnemonicNode(enum MnemonicCode mne);
extern ListNode createAddrModeNode(enum AddrModes addrMode);

extern bool isListNode(ListNode node);
extern bool isToken(ListNode node, enum ParseToken parseToken);

//--- List functions

extern void printParseTreeMemUsage();

extern List * createList(int initialSize);
extern List * condenseList(List *inList);
extern int canAddToList(List *list);
extern int addNode(List *list, ListNode node);
extern void unwarpNodeList(List *nodeList, ListNode *node);
extern void reverseList(List *list);

extern void showNode(FILE *outputFile, ListNode node, int indentLevel);
extern void showList(FILE *outputFile, const List *list, int indentLevel);
extern void destroyList(List *list);

#endif
