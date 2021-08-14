//
// Created by User on 7/25/2021.
//

#ifndef NEOLITHIC_PARSE_DIRECTIVES_H
#define NEOLITHIC_PARSE_DIRECTIVES_H

#include <data/syntax_tree.h>

#define NUM_COMPILER_DIRECTIVES 7

enum CompilerDirectiveTokens {
    UNKNOWN_DIRECTIVE,
    INCLUDE,
    MACHINE_DEF,
    SHOW_CYCLES,
    HIDE_CYCLES,
    PAGE_ALIGN,
    INVERT,
};

extern enum CompilerDirectiveTokens lookupDirectiveToken(char *tokenName);
extern ListNode parse_compilerDirective();

#endif //NEOLITHIC_PARSE_DIRECTIVES_H
