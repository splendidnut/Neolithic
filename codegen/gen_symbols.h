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
// Created by admin on 3/27/2020.
//

#ifndef MODULE_GEN_SYMBOLS_H
#define MODULE_GEN_SYMBOLS_H

#include "data/syntax_tree.h"
#include "data/symbols.h"
#include "machine/mem.h"

extern void generate_symbols(ListNode node, SymbolTable *symbolTable);

#endif //MODULE_GEN_SYMBOLS_H
