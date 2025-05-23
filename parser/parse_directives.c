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
// Created by User on 7/25/2021.
//

#include <string.h>

#include "parse_directives.h"

const char* CompilerDirectiveNames[NUM_COMPILER_DIRECTIVES] = {
        "",
        "include",
        "define",
        "ifdef",
        "ifndef",
        "else",
        "endif",
        "machine",
        "show_cycles",
        "hide_cycles",
        "page_align",
        "invert",
        "reverse",
        "use_quick_index_table",
        "echo",
        "inline",

        // Banking / Address overrides
        "set_bank",
        "set_banking",
        "set_address",
        "always_include"
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

ListNode buildDefineDirective(enum CompilerDirectiveTokens token) {
    List *defineList = createList(4);
    addNode(defineList, createParseToken(PT_DIRECTIVE));
    addNode(defineList, createIntNode(token));
    addNode(defineList, createStrNode(copyTokenStr(getToken())));
    addNode(defineList, parse_expr());
    return createListNode(defineList);
}

ListNode buildIfDirective(enum CompilerDirectiveTokens token) {
    List *dirList = createList(2);
    addNode(dirList, createParseToken(PT_DIRECTIVE));
    addNode(dirList, createIntNode(token));
    return createListNode(dirList);
}

ListNode buildEchoDirective(enum CompilerDirectiveTokens token) {
    List *echoList = createList(20);
    addNode(echoList, createParseToken(PT_DIRECTIVE));
    addNode(echoList, createIntNode(token));
    do {
        addNode(echoList, parse_expr());
    } while (acceptOptionalToken(TT_COMMA));
    return createListNode(echoList);
}

ListNode buildBankDirective(enum CompilerDirectiveTokens token) {
    List *bankDirList = createList(3);
    addNode(bankDirList, createParseToken(PT_DIRECTIVE));
    addNode(bankDirList, createIntNode(token));
    addNode(bankDirList, parse_expr());
    return createListNode(bankDirList);
}

ListNode buildBankingDirective(enum CompilerDirectiveTokens token) {

    char *bankingFlag = getToken()->tokenStr;
    if (bankingFlag != NULL) {
        List *bankDirList = createList(3);
        addNode(bankDirList, createParseToken(PT_DIRECTIVE));
        addNode(bankDirList, createIntNode(token));
        addNode(bankDirList, createStrNode(bankingFlag));
        return createListNode(bankDirList);
    }
    return createEmptyNode();
}


ListNode buildAddrDirective(enum CompilerDirectiveTokens token) {
    List *bankDirList = createList(3);
    addNode(bankDirList, createParseToken(PT_DIRECTIVE));
    addNode(bankDirList, createIntNode(token));
    addNode(bankDirList, parse_expr());
    return createListNode(bankDirList);
}

ListNode buildDirectiveWithOptionalNumeric(enum CompilerDirectiveTokens token) {
    int directiveLineNum = getProgLineNum();
    List *bankDirList = createList(3);
    addNode(bankDirList, createParseToken(PT_DIRECTIVE));
    addNode(bankDirList, createIntNode(token));
    TokenObject *nxtToken = peekToken();
    int nxtTokenLineNum = getProgLineNum();
    if (nxtTokenLineNum == directiveLineNum) {
        addNode(bankDirList, parse_numeric());
    }
    return createListNode(bankDirList);
}


ListNode parse_compilerDirective(enum ParserScope parserScope) {
    ListNode node;
    acceptToken(TT_HASH);   // EAT '#'

    TokenObject *token = getToken();
    enum CompilerDirectiveTokens directiveToken = lookupDirectiveToken(token->tokenStr);

    if (directiveToken != UNKNOWN_DIRECTIVE) {
        switch (directiveToken) {
            case DEFINE:
                node = buildDefineDirective(directiveToken);
                break;
            case IFDEF:
            case IFNDEF:
                node = buildIfDirective(directiveToken);
                break;
            case ECHO:
                node = buildEchoDirective(directiveToken);
                break;
            case SET_BANK:
                node = buildBankDirective(directiveToken);
                break;
            case SET_BANKING:
                node = buildBankingDirective(directiveToken);
                break;
            case SET_ADDRESS:
                node = buildAddrDirective(directiveToken);
                break;
            case PAGE_ALIGN:
                node = buildDirectiveWithOptionalNumeric(directiveToken);
                break;

                // Preprocessor Cases:  These directives have already been processed, so skip them
            case MACHINE_DEF:
            case INCLUDE:
                // TODO: determine if we want to do something special with includes
                node = createEmptyNode();

                // tell tokenizer to skip rest of line (since these directives don't get processed here)
                tokenizer_nextLine();
                break;

                // Default Case:  Just a simple directive, no parameters and no need to ignore
            default:
                node = buildDirectiveNode(directiveToken);
        }
    } else {
        printf("Missing support for #%s\n", token->tokenStr);
        node = createEmptyNode();

        // tell tokenizer to skip rest of line (since there may be parameters that we can't process)
        tokenizer_nextLine();
    }
    return node;
}