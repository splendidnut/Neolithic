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
// Created by User on 9/1/2021.
//

#ifndef NEOLITHIC_TYPE_CHECKER_H
#define NEOLITHIC_TYPE_CHECKER_H

#include <stdbool.h>

#include "data/syntax_tree.h"
#include "data/symbols.h"

bool TypeCheck_Alias(const List *expr, enum SymbolType destType);
bool TypeCheck_PropertyReference(const List *propertyRef, enum SymbolType destType);

#endif //NEOLITHIC_TYPE_CHECKER_H
