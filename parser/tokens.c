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
        "return",
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
        "register",

        "list",

        "directive"
};

const char *getParseTokenName(enum ParseToken pt) { return ParseTokenNames[pt]; }

bool isComparisonToken(enum ParseToken token) {
    return ((token == PT_EQ) || (token == PT_NE)
        || (token == PT_GT)  || (token == PT_GTE)
        || (token == PT_LT)  || (token == PT_LTE));
}