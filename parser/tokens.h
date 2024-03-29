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

#ifndef MODULE_TOKENS_H
#define MODULE_TOKENS_H

#include <stdbool.h>

enum ParseToken {
    PT_EMPTY,
    PT_CODE,            // code block
    PT_FUNCTION,        // define a function (JS style)
    PT_INLINE,          // define an inline function
    PT_FUNC_CALL,
    PT_PROPERTY_REF,
    PT_PROGRAM,         // program block

    PT_DEFINE,          // define a variable/struct
    PT_DEFUN,           // define a function (C style)
    PT_VARS,
    PT_PTR,
    PT_ARRAY,
    PT_INIT,            // init a variable (special: can also handle constants)
    PT_HINT,            // register hint (use A, X, Y)

    PT_DOWHILE,
    PT_WHILE,
    PT_FOR,
    PT_LOOP,
    PT_RETURN,
    PT_BREAK,

    // structure and array support
    PT_STRUCT, PT_UNION, PT_ENUM, PT_LOOKUP,

    // single read-modify-write operations
    PT_BIT_AND, PT_BIT_OR, PT_BIT_EOR, PT_INC, PT_DEC,

    // if and conditionals
    PT_IF,
    PT_EQ,
    PT_NE,
    PT_GT,
    PT_GTE,
    PT_LT,
    PT_LTE,

    // specialty
    PT_SET, PT_CAST, PT_ADDR_OF, PT_DATA_AT,

    // handle inline assembly language
    PT_ASM, PT_LABEL, PT_EQUATE, PT_STROBE, PT_BYTE, PT_WORD,

    // basic math ops
    PT_ADD, PT_SUB,
    PT_SHIFT_LEFT, PT_SHIFT_RIGHT,
    PT_INVERT, PT_NOT, PT_POSITIVE, PT_NEGATIVE,
    PT_BOOL_AND, PT_BOOL_OR,
    PT_LOW_BYTE, PT_HIGH_BYTE,

    // switch statement
    PT_SWITCH, PT_CASE, PT_DEFAULT,

    // specialty math ops
    PT_MULTIPLY, PT_DIVIDE,

    PT_PARAMLIST,

    // modifier tokens
    PT_ZEROPAGE,
    PT_SIGNED,
    PT_UNSIGNED,
    PT_CONST,
    PT_ALIAS,
    PT_REGISTER,

    // mark a list of data values
    PT_LIST,

    // introspective ops
    PT_SIZEOF,
    PT_TYPEOF,

    // make compiler directive
    PT_DIRECTIVE,


    // MARK END OF LIST
    NUM_PARSE_TOKENS
};

extern const char *getParseTokenName(enum ParseToken pt);
extern bool isComparisonToken(enum ParseToken token);

#endif //MODULE_TOKENS_H
