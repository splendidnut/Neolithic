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
// Created by admin on 4/2/2021.
//

#ifndef TREE_WALKER_H
#define TREE_WALKER_H

#include "data/syntax_tree.h"
#include "parser/tokens.h"
#include "data/symbols.h"

//---------------------------------------------------------
// structure to map parse tokens to processing functions

typedef struct {
    enum ParseToken parseToken;
    void (*parseFunc)(const List *, enum SymbolType destType);      // pointer to parse function
} ParseFuncTbl;


typedef void (*ProcessNodeFunc)(ListNode, const List *);      // pointer to parse function


extern int findFunc(ParseFuncTbl function[], int tblSize, enum ParseToken token);
extern void callFunc(ParseFuncTbl functionList[], int tblSize, enum ParseToken token,
              List *dataList, enum SymbolType destType);

#endif //MODULE_TREE_WALKER_H
