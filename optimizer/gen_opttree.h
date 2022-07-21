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
//  Generate Optimized Syntax Tree
//
//   This module is responsible for applying optimizations to the syntax tree before
//   code generation is run.
//
// Created by admin on 4/2/2021.
//

#ifndef MODULE_GEN_OPTTREE_H
#define MODULE_GEN_OPTTREE_H

void optimize_parse_tree(ListNode node);

#endif //MODULE_GEN_OPTTREE_H
