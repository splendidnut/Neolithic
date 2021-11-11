//
// Created by User on 7/25/2021.
//

#ifndef NEOLITHIC_PARSE_DIRECTIVES_H
#define NEOLITHIC_PARSE_DIRECTIVES_H

#include <data/syntax_tree.h>
#include "parser.h"

enum CompilerDirectiveTokens {
    UNKNOWN_DIRECTIVE,
    INCLUDE,
    DEFINE,
    IFDEF,
    IFNDEF,
    ELSE,
    ENDIF,
    MACHINE_DEF,
    SHOW_CYCLES,
    HIDE_CYCLES,
    PAGE_ALIGN,
    INVERT,
    USE_QUICK_INDEX_TABLE,
    ECHO,

    //---- size of list
    NUM_COMPILER_DIRECTIVES
};

extern enum CompilerDirectiveTokens lookupDirectiveToken(char *tokenName);
extern ListNode parse_compilerDirective(enum ParserScope parserScope);

#endif //NEOLITHIC_PARSE_DIRECTIVES_H
