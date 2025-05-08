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
// Created by admin on 6/22/2020.
//

#ifndef MODULE_OPTIMIZER_H
#define MODULE_OPTIMIZER_H

#include "output/output_block.h"

extern void OPT_CodeBlock(OutputBlock *curBlock);
extern void OPT_CheckBranchAlignment(OutputBlock *curBlock);

#endif //MODULE_OPTIMIZER_H
