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
// Created by User on 8/19/2021.
//

#ifndef NEOLITHIC_GEN_ASMCODE_H
#define NEOLITHIC_GEN_ASMCODE_H

#include <data/symbols.h>
#include <data/syntax_tree.h>

extern void GC_AsmBlock(const List *code, enum SymbolType destType);

#endif //NEOLITHIC_GEN_ASMCODE_H
