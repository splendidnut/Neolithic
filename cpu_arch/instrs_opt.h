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
// Created by User on 11/8/2022.
//

#ifndef NEOLITHIC_INSTRS_OPT_H
#define NEOLITHIC_INSTRS_OPT_H

#include "data/syntax_tree.h"
#include "data/symbols.h"

extern void ICG_CopyDataIntoStruct(ListNode listNode, SymbolRecord *destVar, bool useIndirect);

#endif //NEOLITHIC_INSTRS_OPT_H
