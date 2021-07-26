//
// Created by User on 7/25/2021.
//

#include <string.h>
#include "parse_directives.h"
#include "tokenize.h"

const char* CompilerDirectiveNames[NUM_COMPILER_DIRECTIVES] = {
        "",
        "show_cycles",
        "hide_cycles"
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
    printf("compiler directive: %s\n", token->tokenStr);
    enum CompilerDirectiveTokens directiveToken = lookupDirectiveToken(token->tokenStr);

    switch (directiveToken) {
        case SHOW_CYCLES:
            node = buildDirectiveNode(directiveToken);
            break;
        case HIDE_CYCLES:
            node = buildDirectiveNode(directiveToken);
            break;
        default:
            printf("Missing support for %s\n", token->tokenStr);
            node = createEmptyNode();
    }

    // tell tokenizer to skip rest of line
    tokenizer_nextLine();
    return node;
}