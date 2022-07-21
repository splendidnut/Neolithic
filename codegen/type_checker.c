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
//  Type Checker Module
//
//  NOTE:
//  This is an inline type checker.
//  These routines should be run during code generation.
//
// Created by User on 9/1/2021.
//

#include "type_checker.h"
#include "gen_common.h"

// TODO: make this checker a bit more robust:
//        - check against destination type
bool TypeCheck_Primitive(ListNode primitive, enum SymbolType destType, int lineNum) {
    // num or str
    switch (primitive.type) {
        case N_STR: {
            SymbolRecord *varRef = lookupSymbolNode(primitive, lineNum);
            return (varRef != NULL);
        }
        case N_INT: {
            return true;
        }
        default:
            return false;
    }
}

bool TypeCheck_Alias(const List *expr, enum SymbolType destType) {
    ListNode opNode = expr->nodes[0];
    if (isToken(opNode, PT_LOOKUP)) {
        SymbolRecord *arraySymbol = lookupSymbolNode(expr->nodes[1], expr->lineNum);
        if (!arraySymbol) return false;

        return TypeCheck_Primitive(expr->nodes[2], destType, expr->lineNum);
    } else if (isToken(opNode, PT_INIT)) {

        // we have a simple variable alias
        return TypeCheck_Primitive(expr->nodes[1], destType, expr->lineNum);

    } else {
        return false;
    }
}

bool TypeCheck_PropertyReference(const List *propertyRef, enum SymbolType destType) {
    SymbolRecord *structSym = lookupSymbolNode(propertyRef->nodes[1], propertyRef->lineNum);
    if (structSym == NULL) {
        return false;
    }

    ListNode propertyNode = propertyRef->nodes[2];
    if (propertyNode.type != N_STR) {
        ErrorMessageWithNode("Invalid property - not an identifier", propertyNode, propertyRef->lineNum);
        return false;
    }

    return true;
}