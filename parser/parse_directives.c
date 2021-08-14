//
// Created by User on 7/25/2021.
//

#include <string.h>
#include "parse_directives.h"
#include "tokenize.h"

const char* CompilerDirectiveNames[NUM_COMPILER_DIRECTIVES] = {
        "",
        "include",
        "machine",
        "show_cycles",
        "hide_cycles",
        "page_align",
        "invert"
};

enum CompilerDirectiveTokens lookupDirectiveToken(char *tokenName) {
    int index;
    for (index=1; index < NUM_COMPILER_DIRECTIVES; index++) {
        if (strncmp(tokenName, CompilerDirectiveNames[index], TOKEN_LENGTH_LIMIT) == 0) break;
    }
    if (index >= NUM_COMPILER_DIRECTIVES) index = 0;
    return (enum CompilerDirectiveTokens)index;
}


ListNode buildDirectiveNode(enum CompilerDirectiveTokens token) {
    List *dirList = createList(2);
    addNode(dirList, createParseToken(PT_DIRECTIVE));
    addNode(dirList, createIntNode(token));
    return createListNode(dirList);
}


ListNode parse_compilerDirective() {
    ListNode node;
    getToken(); // EAT '#'

    TokenObject *token = getToken();
    enum CompilerDirectiveTokens directiveToken = lookupDirectiveToken(token->tokenStr);

    switch (directiveToken) {
        case SHOW_CYCLES:
        case HIDE_CYCLES:
        case PAGE_ALIGN:
        case INVERT:
            node = buildDirectiveNode(directiveToken);
            break;
        case MACHINE_DEF:
        case INCLUDE:
            // TODO: determine if we want to do something special with includes
            node = createEmptyNode();
            break;
        default:
            printf("Missing support for %s\n", token->tokenStr);
            node = createEmptyNode();
    }

    // tell tokenizer to skip rest of line
    tokenizer_nextLine();
    return node;
}