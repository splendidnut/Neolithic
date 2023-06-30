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
// Created by admin on 10/17/2020.
//

#ifndef MODULE_GEN_CALLTREE_H
#define MODULE_GEN_CALLTREE_H

#include "data/symbols.h"
#include "data/syntax_tree.h"

extern void generate_callTree(ListNode node, SymbolTable *symbolTable, bool isMain);

#endif //MODULE_GEN_CALLTREE_H
