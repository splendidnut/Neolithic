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
// Created by admin on 4/3/2020.
//

#include <stdbool.h>

#include "tokens.h"

const char* ParseTokenNames[NUM_PARSE_TOKENS] = {
        "",
        "code",
        "function",
        "inline",
        "funcCall",
        "propertyRef",
        "program",
        "define",
        "defun",
        "varList",
        "pointer",
        "array",
        "init",
        "hint",     // compiler hint (mainly for which register to use)

        "doWhile",
        "while",
        "for",
        "loop",
        "return",
        "break",
        "struct",
        "union",
        "enum",
        "lookup",
        "bit_and",
        "bit_or",
        "bit_eor",
        "inc",
        "dec",

        "if",
        "equal",
        "notEqual",
        "greaterThan",
        "greaterThanEqual",
        "lessThan",
        "lessThanEqual",

        "set",
        "cast",
        "addrOf",
        "dataAt",

        "asm",
        "label",
        "equate",
        "strobe",
        "byte",
        "word",

        "add",
        "sub",
        "shiftLeft",
        "shiftRight",

        "bit_invert",
        "not",
        "pos",
        "neg",

        "bool_and",
        "bool_or",
        "low_byte",
        "high_byte",

        "switch",
        "case",
        "default",

        "mul",
        "div",

        "params",

        "zeropage",
        "signed",
        "unsigned",
        "const",
        "alias",
        "register",

        "list",

        "sizeof",
        "typeof",

        "directive"
};

const char *getParseTokenName(enum ParseToken pt) { return ParseTokenNames[pt]; }

bool isComparisonToken(enum ParseToken token) {
    return ((token == PT_EQ) || (token == PT_NE)
        || (token == PT_GT)  || (token == PT_GTE)
        || (token == PT_LT)  || (token == PT_LTE));
}