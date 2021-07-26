//
// Created by User on 7/25/2021.
//

#ifndef NEOLITHIC_PARSE_DIRECTIVES_H
#define NEOLITHIC_PARSE_DIRECTIVES_H

#include <data/syntax_tree.h>

#define NUM_COMPILER_DIRECTIVES 3

enum CompilerDirectiveTokens {
    UNKNOWN_DIRECTIVE,
    SHOW_CYCLES,
    HIDE_CYCLES
};


extern ListNode parse_compilerDirective();

#endif //NEOLITHIC_PARSE_DIRECTIVES_H
