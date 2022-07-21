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
// .Neolithic Compiler Module - Flatten Tree
//
//  This module is used for flattening an expression tree to allow
//  easier implementation of 16-bit code generation.
//
// Created by pblackman on 8/3/21.
//

#ifndef NEOLITHIC_FLATTEN_TREE_H
#define NEOLITHIC_FLATTEN_TREE_H

#include <data/syntax_tree.h>

extern List* flatten_expression(const List *expr, const List *origStmt);

extern void FE_initDebugger();
extern void FE_killDebugger();

#endif //NEOLITHIC_FLATTEN_TREE_H
