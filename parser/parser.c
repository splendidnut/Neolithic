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

/*
 *   Neolithic Parser
 *
 *   Recursive-descent style parser
 *
 *   TODO: Fix issues with unary style operators.
 *          Unary ops should be parsed right before primitives.
 *          The AddrOf '&' already is.  It's currently done inside
 *          the primary expression routine (parenthesis / primitive)
 *          handling which is not quite the right place for it.
 *
 *   TODO: Fix negative/positive signs (part of the previous todo)
 *
 *   TODO: Fix "magic" numbers, i.e. clarify numeric constants...
 *
**/

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data/syntax_tree.h"
#include "data/type_list.h"
#include "tokens.h"
#include "tokenize.h"
#include "parser.h"
#include "parse_asm.h"
#include "parse_directives.h"

// These define the largest lists allocated during parsing
#define MAX_STATEMENTS_IN_BLOCK 2000
#define MAX_PROG_DECLARATIONS 1000
#define STRUCT_VAR_LIMIT 200
#define MAX_ITEMS_IN_LIST 257
#define MAX_SWITCH_CASES 32

//---------------------------
//  Global variables

int parserErrorCount;

/*-----------------------
// general functions
*/
const int ERR_MSG_SIZE = 120;
const int MAX_PARSER_ERRORS = 3;
static int errorCount = 0;
static bool noMoreErrors = false;
void printError(const char* fmtErrorMsg, ...) {
    if (noMoreErrors) return;

    if (errorCount < MAX_PARSER_ERRORS) {
        char errorMsg[ERR_MSG_SIZE];

        va_list argList;
        va_start(argList, fmtErrorMsg);
        vsnprintf(errorMsg, ERR_MSG_SIZE, fmtErrorMsg, argList);
        va_end(argList);

        printf("ERROR on line %d:  %s\n", getProgLineNum(), errorMsg);
        errorCount++;
    } else if (errorCount == MAX_PARSER_ERRORS) {
        printf("NOTE: Error limit exceeded.  No more errors will be reported.\n\n");
        noMoreErrors = true;
    }
}

void printErrorWithSourceLine(const char* errorMsg) {
    if (noMoreErrors) return;

    if (errorCount < MAX_PARSER_ERRORS) {
        char sourceLine[ERR_MSG_SIZE];
        SourceCodeLine sourceCodeLine = getProgramLineString();

        snprintf(sourceLine, sourceCodeLine.len, "%s", sourceCodeLine.data);

        printf("ERROR on line %d:  %s\n\t%s\n", getProgLineNum(), errorMsg, sourceLine);
        errorCount++;
    } else if (errorCount == MAX_PARSER_ERRORS) {
        printf("NOTE: Error limit exceeded.  No more errors will be reported.\n\n");
        noMoreErrors = true;
    }
}


bool acceptToken(TokenType tokenType) {
    TokenObject *token = getToken();
    bool tokenMatch = (token->tokenType == tokenType);
    if (!tokenMatch) {
        const char *tokenStr = lookupTokenSymbol(tokenType);
        printError("Unexpected token: '%s' -- was looking for: '%s'\n", token->tokenStr, tokenStr);
    }
    return tokenMatch;
}

bool acceptOptionalToken(TokenType tokenType) {
    bool foundToken = (peekToken()->tokenType == tokenType);
    if (foundToken) getToken();
    return foundToken;
}

/*------------------------
//   Parsing functions
*/

ListNode parse_identifier() {
    ListNode node = createIntNode(0);
    TokenObject *token = getToken();
    if (token->tokenType == TT_IDENTIFIER) {
        char *identifier = copyTokenStr(token);
        node = createStrNode(identifier);
    } else {
        printErrorWithSourceLine("Identifier expected!\n");
    }
    return node;
}

bool isClosingListToken(TokenObject *token) {
    TokenType tokenType = token->tokenType;
    return ((tokenType == TT_CLOSE_BRACE) || (tokenType == TT_CLOSE_BRACKET));
}

ListNode parse_list() {
    List *items = createList(MAX_ITEMS_IN_LIST);
    addNode(items, createParseToken(PT_LIST));

    while (!isClosingListToken(peekToken())) {
        ListNode node = parse_expr();
        addNode(items, node);
        acceptOptionalToken(TT_COMMA);
    }
    getToken(); // -- eat closing token

    items = condenseList(items);
    return createListNode(items);
}

ListNode parse_cast_op() {
    List *castingList = createList(10);

    // capture all casting tokens
    while (peekToken()->tokenType != TT_CLOSE_PAREN) {
        char *castToken = copyTokenStr(getToken());
        addNode(castingList, createStrNode(castToken));
    }
    acceptToken(TT_CLOSE_PAREN);
    return createListNode(castingList);
}

ListNode parse_cast_expr() {
    ListNode node;
    List *castExprList = createList(3);
    addNode(castExprList, createParseToken(PT_CAST));
    addNode(castExprList, parse_cast_op());

    // if parenthesis follows, parse that expression
    if (peekToken()->tokenType == TT_OPEN_PAREN) {
        acceptToken(TT_OPEN_PAREN);
        addNode(castExprList, parse_expr());
        acceptToken(TT_CLOSE_PAREN);
    } else {
        addNode(castExprList, parse_expr());
    }
    node = createListNode(castExprList);
    return node;
}

ListNode parse_nested_primary(int allowNestedExpr, TokenType tokenType) {
    ListNode node = createEmptyNode();
    switch (tokenType) {
        case TT_OPEN_PAREN:
            if (allowNestedExpr) {
                //printf("found parenthetical expression\n");
                TokenObject *ptoken = peekToken();
                if ((isTypeToken(ptoken) || isModifierToken(ptoken))) {
                    node = parse_cast_expr();
                } else {
                    node = parse_expr();
                    acceptToken(TT_CLOSE_PAREN);
                }
            } else {
                printError("nested expression not allowed here\n");
            }
            break;
        case TT_OPEN_BRACKET:
        case TT_OPEN_BRACE:
            // found list
            node = parse_list();
            break;
        default: break;
    }
    return node;
}

/**
 * Parse primitive (both simple and more complex like list of values) or nested expression
 * @param isLValue
 * @param isExprAllowed
 * @param allowNestedExpr
 * @return
 */
ListNode parse_primary_expr(bool isLValue, bool isExprAllowed, int allowNestedExpr) {
    ListNode node = createEmptyNode();
    char *identifier;

    TokenObject *token = getToken();
    TokenType exprType = token->tokenType;
    char *tokenStr = token->tokenStr;

    //  eat/handle unary +/- tokens
    bool isNegative = false;
    if ((exprType == TT_MINUS) || (exprType == TT_PLUS)) {
        isNegative = (exprType == TT_MINUS);

        token = getToken();
        exprType = token->tokenType;
        tokenStr = token->tokenStr;
    }

    switch(exprType) {
        case TT_NUMBER:      /* numeric const */
            if (!isLValue) {
                int number = copyTokenInt(token);
                if (isNegative) number = -number;
                node = createIntNode(number);
            } else {
                printError("Improper start of statement: \"%s\"... must be an identifier\n", tokenStr);
            }
            break;

        case TT_TRUE:
        case TT_FALSE:
        case TT_IDENTIFIER:
            identifier = copyTokenStr(token);
            if (isNegative) {
                List *negList = createList(2);
                addNode(negList, createParseToken(PT_NEGATIVE));
                addNode(negList, createStrNode(identifier));
                node = createListNode(negList);
            } else {
                node = createStrNode(identifier);
            }
            break;

        case TT_STRING:
            // handle string literals
            node = createStrLiteralNode(getUnquotedString(tokenStr));
            break;

        case TT_AMPERSAND:
            if (!isLValue) {
                List *list = createList(2);
                addNode(list, createParseToken(PT_ADDR_OF));
                addNode(list, parse_primary_expr(isLValue, isExprAllowed, allowNestedExpr));
                node = createListNode(list);
            } else {
                printError("Cannot use '&' on left side of assignment expression: %s\n", tokenStr);
            }
            break;

            //----- NOTE: TT_OPEN_BRACE falls thru to TT_OPEN_PAREN
        case TT_OPEN_BRACE:
            if (isLValue) break;
        case TT_OPEN_PAREN:
            if (isExprAllowed) {
                node = parse_nested_primary(allowNestedExpr, exprType);
                if (isNegative) {
                    List *negList = createList(2);
                    addNode(negList, createParseToken(PT_NEGATIVE));
                    addNode(negList, node);
                    node = createListNode(negList);
                }
            } else {
                printError("Expression not allowed here");
            }
            break;

        case TT_SIZEOF: node = createParseToken(PT_SIZEOF); break;
        case TT_TYPEOF: node = createParseToken(PT_TYPEOF); break;

        default:
            printError("Primitive not found....found token '%s' instead\n", tokenStr);
            node = createEmptyNode();
    }
    return node;
}

ListNode parse_arguments() {
    if (peekToken()->tokenType != TT_CLOSE_PAREN) {
        List *list = createList(20);
        addNode(list, parse_expr());

        while (peekToken()->tokenType == TT_COMMA) {
            acceptToken(TT_COMMA);
            addNode(list, parse_expr());
        }
        list = condenseList(list);
        return createListNode(list);
    } else {
        return createEmptyNode();
    }
}

ListNode parse_expr_size_type(ListNode opNode) {
    List *specExpr = createList(2);
    addNode(specExpr, opNode);
    acceptToken(TT_OPEN_PAREN);
    addNode(specExpr, parse_identifier());
    acceptToken(TT_CLOSE_PAREN);
    return createListNode(specExpr);
}

//--------------------------------------------------------------------
//   Parse postfix expressions:
//       primitive
//       postfixExpr [ expr ]           -- array/object lookup
//       postfixExpr ( argumentList )   -- function call
//       postfixExpr . identifier       -- reference object property
//       postfixExpr ++ / --
//       sizeof/typeof ( var/type identifer )

ListNode parse_expr_postfix(bool isLValue, int allowNestedExpr) {
    ListNode lnode, rnode, opNode;
    List *list = NULL;
    char tokenChar;

    lnode = parse_primary_expr(isLValue, true, allowNestedExpr);
    if (lnode.type == N_INT) return lnode;                          // Numeric values cannot have postfix ops applied

    // if lnode is token... then it's probably sizeof/typeof,
    //   so call a function to process that and return the result
    if (lnode.type == N_TOKEN) return parse_expr_size_type(lnode);

    while (inCharset(peekToken(), "[(.")) {
        tokenChar = getToken()->tokenStr[0];

        // handle right node depending on which path we're taking;
        switch (tokenChar) {
            case '[':
                if (peekToken()->tokenType != TT_CLOSE_BRACKET) {
                    rnode = parse_expr();
                    acceptToken(TT_CLOSE_BRACKET);
                    opNode = createParseToken(PT_LOOKUP);
                } else {
                    // TODO: this is only useful for var definitions
                    acceptToken(TT_CLOSE_BRACKET);
                    opNode = createParseToken(PT_ARRAY);
                    rnode = createEmptyNode();
                }
                break;
            case '(':
                rnode = parse_arguments();
                acceptToken(TT_CLOSE_PAREN);
                opNode = createParseToken(PT_FUNC_CALL);
                break;
            case '.':
                rnode = parse_identifier();
                opNode = createParseToken(PT_PROPERTY_REF);
                break;
            default: break;
        }
        list = createList(3);
        addNode(list, opNode);
        addNode(list, lnode);
        addNode(list, rnode);

        // for next pass, lnode becomes the list we just created
        lnode = createListNode(list);
    }

    // check for inc/dev operator
    if (isIncDec(peekToken()->tokenType)) {
        switch (getToken()->tokenType) {
            case TT_INC: opNode = createParseToken(PT_INC); break;
            case TT_DEC: opNode = createParseToken(PT_DEC); break;
            default: break;
        }

        list = createList(2);
        addNode(list, opNode);
        addNode(list, lnode);

        lnode = createListNode(list);
    }

    return lnode;   // return either the primitive node or a list node
}

//---------------------------------------------------------------------
//  Parse unary expressions
//     +expr, -expr, *expr, &expr

ListNode parse_expr_unary() { // !x +x -y &x *y sizeof typeof
    ListNode node, opNode;
    List *list;
    char tokenChar;

    if (inCharset(peekToken(), "!+*~<>")) {
        tokenChar = getToken()->tokenStr[0];
        switch (tokenChar) {
            //case '&': opNode = createParseToken(PT_ADDR_OF); break;
            case '!': opNode = createParseToken(PT_NOT); break;
            case '+': opNode = createParseToken(PT_POSITIVE); break;
            //
            // case '-': opNode = createParseToken(PT_NEGATIVE); break;
            case '~': opNode = createParseToken(PT_INVERT); break;
            case '<': opNode = createParseToken(PT_LOW_BYTE); break;
            case '>': opNode = createParseToken(PT_HIGH_BYTE); break;
            default:
                opNode = createCharNode(tokenChar);
        }

        node = parse_expr_postfix(false, 1);

        list = createList(2);
        addNode(list, opNode);
        addNode(list, node);

        node = createListNode(list);
    } else {
        node = parse_expr_postfix(false, 1);
    }
    return node;
}

/* //  These are all very similar
ListNode parse_expr_mulDiv(void) {} // * /
ListNode parse_expr_addSub(void) {} // + -
ListNode parse_expr_shift(void) {} // >> <<
ListNode parse_expr_bitwise(void) {} // & | ^
ListNode parse_expr_comparison(void) {} // == != < <= > >=
ListNode parse_expr_logical(void) {} // && ||
ListNode parse_expr_conditonal(void) {}  //  x ? y : z
*/

ListNode parse_expr_mulDiv() {  // * /
    ListNode lnode, rnode, opNode;
    List *list;

    lnode = parse_expr_unary();
    while (inCharset(peekToken(), "*/")) {
        TokenType tokenType = getToken()->tokenType;
        switch (tokenType) {
            case TT_MULTIPLY: opNode = createParseToken(PT_MULTIPLY); break;
            case TT_DIVIDE:   opNode = createParseToken(PT_DIVIDE);   break;
            default: opNode = createEmptyNode();
        }
        rnode = parse_expr_unary();

        list = createList(3);
        addNode(list, opNode);
        addNode(list, lnode);
        addNode(list, rnode);

        // for next pass, lnode becomes the list we just created
        lnode = createListNode(list);
    }
    return lnode;
}


ListNode parse_expr_addSub() { // + -
    ListNode lnode, rnode, opNode;
    List *list;

    lnode = parse_expr_mulDiv();
    while (inCharset(peekToken(), "+-")) {
        TokenType tokenType = getToken()->tokenType;
        switch (tokenType) {
            case TT_PLUS:
                opNode = createParseToken(PT_ADD);
                break;
            case TT_MINUS:
                opNode = createParseToken(PT_SUB);
                break;
            default:
                opNode = createEmptyNode();
        }
        rnode = parse_expr_mulDiv();

        list = createList(3);
        addNode(list, opNode);
        addNode(list, lnode);
        addNode(list, rnode);

        // for next pass, lnode becomes the list we just created
        lnode = createListNode(list);
    }
    return lnode;
}

ListNode parse_expr_shift() { // >> <<
    ListNode lnode, rnode, opNode;
    List *list;

    lnode = parse_expr_addSub();
    while (isShift(peekToken()->tokenType)) {
        switch (getToken()->tokenType) {
            case TT_SHIFT_LEFT: opNode = createParseToken(PT_SHIFT_LEFT); break;
            case TT_SHIFT_RIGHT: opNode = createParseToken(PT_SHIFT_RIGHT); break;
            default: break;
        }
        rnode = parse_expr_addSub();

        list = createList(3);
        addNode(list, opNode);
        addNode(list, lnode);
        addNode(list, rnode);

        // for next pass, lnode becomes the list we just created
        lnode = createListNode(list);
    }
    return lnode;
}

ListNode parse_expr_bitwise() { // & | ^
    ListNode lnode, rnode, opNode;
    List *list;

    lnode = parse_expr_shift();
    while (inCharset(peekToken(), "&|^")) {
        switch (getToken()->tokenStr[0]) {
            case '&': opNode = createParseToken(PT_BIT_AND); break;
            case '|': opNode = createParseToken(PT_BIT_OR); break;
            case '^': opNode = createParseToken(PT_BIT_EOR); break;
            default: break;
        }
        rnode = parse_expr_shift();

        list = createList(3);
        addNode(list, opNode);
        addNode(list, lnode);
        addNode(list, rnode);

        // for next pass, lnode becomes the list we just created
        lnode = createListNode(list);
    }
    return lnode;
}

ListNode parse_expr_comparison() { // == != < <= > >=
    ListNode lnode, rnode, opNode;
    List *list;
    TokenObject *token;

    lnode = parse_expr_bitwise();
    while (isComparison(peekToken())) {
        token = getToken();

        enum ParseToken parseToken = PT_EMPTY;
        switch (token->tokenType) {
            case TT_EQUAL:          parseToken = PT_EQ; break;
            case TT_NOT_EQUAL:      parseToken = PT_NE; break;
            case TT_GREATER_THAN:   parseToken = PT_GT; break;
            case TT_GREATER_EQUAL:  parseToken = PT_GTE; break;
            case TT_LESS_THAN:      parseToken = PT_LT; break;
            case TT_LESS_EQUAL:     parseToken = PT_LTE; break;
            default: break;
        }

        opNode = createParseToken(parseToken);
        rnode = parse_expr_bitwise();

        list = createList(3);
        addNode(list, opNode);
        addNode(list, lnode);
        addNode(list, rnode);

        // for next pass, lnode becomes the list we just created
        lnode = createListNode(list);
    }
    return lnode;
}
ListNode parse_expr_logical() { // && ||
    ListNode lnode, rnode, opNode;
    List *list;

    lnode = parse_expr_comparison();
    while (isBoolAndOr(peekToken()->tokenType)) {
        TokenObject *token = getToken();
        switch (token->tokenType) {
            case TT_BOOL_AND: opNode = createParseToken(PT_BOOL_AND); break;
            case TT_BOOL_OR:  opNode = createParseToken(PT_BOOL_OR); break;
            default:
                opNode = createStrNode(lookupTokenSymbol(token->tokenType));
        }
        rnode = parse_expr_comparison();

        list = createList(3);
        addNode(list, opNode);
        addNode(list, lnode);
        addNode(list, rnode);

        // for next pass, lnode becomes the list we just created
        lnode = createListNode(list);
    }
    return lnode;
}


ListNode parse_expr_conditonal() {  //  x ? y : z
    ListNode lnode, mnode, rnode, opNode;
    List *list;

    lnode = parse_expr_logical();
    if (peekToken()->tokenType == TT_QUESTION) {
        acceptToken(TT_QUESTION);
        opNode = createStrNode("ifExpr");
        mnode = parse_expr_logical();
        acceptToken(TT_COLON);
        rnode = parse_expr();

        list = createList(4);
        addNode(list, opNode);
        addNode(list, lnode);
        addNode(list, mnode);
        addNode(list, rnode);

        // for next pass, lnode becomes the list we just created
        lnode = createListNode(list);
    }
    return lnode;
}

//-------------------------------------------------------
// --- Entry to expression parsing

ListNode parse_expr() {
    return parse_expr_conditonal();
}

ListNode parse_expr_assignment() {
    ListNode lnode, rnode, opNode;
    List *list;

    lnode = parse_expr_postfix(true, 0);  // parse lvalue

    // check if assignment OR just a definition or function call
    bool isBasicAsgnEqual = (peekToken()->tokenType == TT_ASSIGN);
    bool isAsgnOpEqual = isOpAsgn(peekToken()->tokenType);

    if (isBasicAsgnEqual || isAsgnOpEqual) {

        list = createList(3);

        if (isBasicAsgnEqual) {
            acceptToken(TT_ASSIGN);
            opNode = createParseToken(PT_SET);
        } else {
            //--- Handle special assignment:  += -=
            switch (getToken()->tokenType) {
                case TT_ADD_TO:
                    opNode = createParseToken(PT_ADD);
                    break;
                case TT_SUB_FROM:
                    opNode = createParseToken(PT_SUB);
                    break;
                case TT_AND_WITH:
                    opNode = createParseToken(PT_BIT_AND);
                    break;
                case TT_OR_WITH:
                    opNode = createParseToken(PT_BIT_OR);
                    break;
                default: break;
            }
        }
        rnode = parse_expr();

        /* for add_to/sub_from, need to build sub expression*/
        if (isAsgnOpEqual) {
            List *innerList = createList(3);
            addNode(innerList, opNode);
            addNode(innerList, lnode);
            addNode(innerList, rnode);
            rnode = createListNode(innerList);
        }

        addNode(list, createParseToken(PT_SET));
        addNode(list, lnode);
        addNode(list, rnode);
        lnode = createListNode(list);
    }
    return lnode;
}

ListNode parse_var_assignment() {
    List *list = createList(3);

    acceptToken(TT_ASSIGN);
    addNode(list, createParseToken(PT_INIT));
    addNode(list, parse_expr());
    return createListNode(list);
}


//----------------------------------------------------------------------------------------
//  Parse variables:
//     (modifier)* varType *varName[], *varName[]
//
//  multiple list of modifiers 0+
//  single varType is base data type
//  each varName can be a pointer and/or array also
//
//  char *data;     // pointer to character data  (8-bit)
//                          -- can be pointer to array of char
//  int *data;      // pointer to integer data   (16-bit)
//  char data[];    // array of character data
//  char *data[];   // an array of pointers to character data

ListNode parse_func_parameters(void);

ListNode parse_array_node() {

    // exit early if there are no brackets.
    if (peekToken()->tokenType != TT_OPEN_BRACKET) return createEmptyNode();

    ListNode arrayNode = createEmptyNode();
    acceptToken(TT_OPEN_BRACKET);
    if (peekToken()->tokenType != TT_CLOSE_BRACKET) {
        TokenObject *token = peekToken();
        if (token->tokenType == TT_NUMBER) {
            int arraySize = copyTokenInt(getToken());
            arrayNode = createIntNode(arraySize);
        } else {
            arrayNode = parse_expr();
        }
    }
    acceptToken(TT_CLOSE_BRACKET);

    // create array node as a list:  (array, *size*)
    List *arrayList = createList(2);
    addNode(arrayList, createParseToken(PT_ARRAY));
    addNode(arrayList, arrayNode);
    return createListNode(arrayList);
}

ListNode parse_var_node(const char *baseType, const List *modList, const char *regHint) {
    bool isFunctionDef = false;
    ListNode varNameNode, asgnNode, paramNode;
    List *list = createList(6);
    List *typeList = createList(3);

    addNode(typeList, createStrNode(baseType));

    // handle memory hint
    bool hasMemHint = acceptOptionalToken(TT_AT_SIGN);
    int memAddr = hasMemHint ? copyTokenInt(getToken()) : 0;

    // handle pointer reference
    if (acceptOptionalToken(TT_MULTIPLY)) {
        addNode(typeList, createParseToken(PT_PTR));
    }

    // capture name
    varNameNode = createStrNode(copyTokenStr(getToken()));

    // handle array indicator
    ListNode arrayNode = parse_array_node();
    if (arrayNode.type != N_EMPTY) {
        addNode(typeList, arrayNode);
    }

    // handle regHint
    if (regHint) {
        List *regHintList = createList(2);
        addNode(regHintList, createParseToken(PT_HINT));
        addNode(regHintList, createStrNode(regHint));
        addNode(typeList, createListNode(regHintList));
    }

    // handle function declarator
    if (peekToken()->tokenType == TT_OPEN_PAREN) {
        paramNode = parse_func_parameters();
        isFunctionDef = true;
    } else {
        paramNode = createEmptyNode();
    }

    // handle initializer
    asgnNode = ((peekToken()->tokenType == TT_ASSIGN) ? parse_var_assignment() : createEmptyNode());

    // build a variable node
    //list = createList(6);

    addNode(list, createParseToken(isFunctionDef ? PT_DEFUN : PT_DEFINE));
    addNode(list, varNameNode);
    addNode(list, createListNode(typeList));
    addNode(list, (modList != NULL) ? createListNode((List *) modList) : createEmptyNode());
    addNode(list, isFunctionDef ? paramNode : asgnNode);
    if (hasMemHint) {
        addNode(list, createIntNode(memAddr));
    }

    if (isFunctionDef && (peekToken()->tokenType == TT_OPEN_BRACE || peekToken()->tokenType == TT_ASM)) {
        addNode(list, parse_codeBlock());
    }

    return createListNode(list);
}

List *parse_mod_list() {// Process all modifiers
    List *modList = createList(5);
    while (isModifierToken(peekToken()))
    {
        // create a copy of each modifier and stick it in the list
        TokenObject *modToken = getToken();
        enum ParseToken parseToken = PT_EMPTY;
        switch (modToken->tokenType) {
            case TT_CONST:    parseToken = PT_CONST;    break;
            case TT_ALIAS:    parseToken = PT_ALIAS;    break;
            case TT_ZEROPAGE: parseToken = PT_ZEROPAGE; break;
            case TT_SIGNED:   parseToken = PT_SIGNED;   break;
            case TT_UNSIGNED: parseToken = PT_UNSIGNED; break;
            case TT_REGISTER: parseToken = PT_REGISTER; break;
            case TT_INLINE:   parseToken = PT_INLINE;   break;
            default:
                printError("Unknown modifier: %s\n", modToken->tokenStr);
                break;
        }
        if (parseToken != PT_EMPTY) {
            addNode(modList, createParseToken(parseToken));
        }
    }
    if (modList->count == 0) {
        destroyList(modList);
        return NULL;
    }
    return modList;
}

ListNode parse_variable(void) {
    List *modList = parse_mod_list();
    char *baseType = copyTokenStr(getToken());

    // parse first variable
    ListNode varNode = parse_var_node(baseType, modList, NULL);

    //----- at this point, we can have multiple parameters separated by commas
    if (peekToken()->tokenType == TT_COMMA) {
        List *varList = createList(20);
        addNode(varList, varNode);
        while (acceptOptionalToken(TT_COMMA)) {
            varNode = parse_var_node(baseType, modList, NULL);
            addNode(varList, varNode);
        }
        return createListNode(varList);
    } else {
        return varNode;
    }
}

ListNode parse_parameter() {
    TokenObject *token = peekToken();

    List *modList = NULL;
    if (isModifierToken(token)) {
        modList = parse_mod_list();
        token = peekToken();
    }

    if (isTypeToken(token) ||
        (isIdentifier(token) && TypeList_find(token->tokenStr) != NULL)) {
        // handle mod list
        char *baseType = copyTokenStr(getToken());
        char *regHint = NULL;
        if (acceptOptionalToken(TT_AT_SIGN)) {
            // it's a register hint
            regHint = copyTokenStr(getToken());
        }
        return parse_var_node(baseType, modList, regHint);
    } else {
        printError("Unknown or missing type: %s\n", token->tokenStr);
        return createEmptyNode();
    }
}

ListNode parse_parameters() {
    List *list = createList(20);
    addNode(list, createParseToken(PT_PARAMLIST));
    addNode(list, parse_parameter());

    while (acceptOptionalToken(TT_COMMA)) {
        addNode(list, parse_parameter());
    }

    return createListNode(list);
}

ListNode parse_func_parameters(void) {
    ListNode funcParams;
    acceptToken(TT_OPEN_PAREN);
    TokenType tokenType = peekToken()->tokenType;
    bool isVoid = acceptOptionalToken(TT_VOID);         // TODO: Potentially remove this
    if (!isVoid && (tokenType != TT_CLOSE_PAREN)) {
        funcParams = parse_parameters();
    } else {
        funcParams = createEmptyNode();
    }
    acceptToken(TT_CLOSE_PAREN);
    return funcParams;
}

//-------------------------------------
//   Enumeration parsing
//-------------------------------------
//    enum EnumTag { var = value, ... }

ListNode parse_enum() {
    int currentEnumValue = 0;           // set initial enum value
    List *enumList = createList(MAX_ITEMS_IN_LIST);
    acceptToken(TT_ENUM);
    addNode(enumList, createParseToken(PT_ENUM));

    //-- check if enumeration has a name
    if (peekToken()->tokenType != TT_OPEN_BRACE) {
        char *enumName = copyTokenStr(getToken());
        addNode(enumList, createStrNode(enumName));  // enum name
        TypeList_add(enumName);
    }

    acceptToken(TT_OPEN_BRACE);
    while (peekToken()->tokenType != TT_CLOSE_BRACE) {
        List *enumNode = createList(2);
        char *valueName = copyTokenStr(getToken());

        // did user set enumeration value?
        if (acceptOptionalToken(TT_ASSIGN)) {
            currentEnumValue = copyTokenInt(getToken());
        }

        // build the node
        addNode(enumNode, createStrNode(valueName));
        addNode(enumNode, createIntNode(currentEnumValue));
        addNode(enumList, createListNode(enumNode));
        currentEnumValue++;

        acceptOptionalToken(TT_COMMA);
    }
    acceptToken(TT_CLOSE_BRACE);

    enumList = condenseList(enumList);
    return createListNode(enumList);
}

//---------------------------------------------------------------------
//   Structure/Union parsing
//---------------------------------------------------------------------
//    struct StructTag;
//    struct { var; var; };
//    struct StructTag { var; var; } varName/typeName;
//    union { var; var };
//    union UnionTag { var; var; } varName/typeName;
//


ListNode parse_structOrUnion(enum ParseToken typeToken);     // Forward Declaration


ListNode parse_struct_vars() {
    List *varList = createList(STRUCT_VAR_LIMIT);
    ListNode node;

    addNode(varList, createParseToken(PT_VARS));

    while (peekToken()->tokenType != TT_CLOSE_BRACE) {
        TokenObject *token = peekToken();
        switch (token->tokenType) {
            case TT_STRUCT:
                printError("Structure definitions cannot be nested");
                node = parse_structOrUnion(PT_STRUCT);
                break;

            case TT_UNION:
                node = parse_structOrUnion(PT_UNION);
                break;

            default:
                node = parse_variable();
        }

        unwarpNodeList(varList, &node);

        acceptOptionalToken(TT_SEMICOLON);
    }
    varList = condenseList(varList);
    return createListNode(varList);
}

ListNode parse_structOrUnion(enum ParseToken typeToken) {
    List *structBase = createList(3);

    getToken(); //-- eat typeToken
    addNode(structBase, createParseToken(typeToken));

    bool hasName = false;
    if (peekToken()->tokenType != TT_OPEN_BRACE) {
        char *typeName = copyTokenStr(getToken());
        TypeList_add(typeName);
        addNode(structBase, createStrNode(typeName));
        hasName = true;
    } else {
        addNode(structBase, createEmptyNode()); // struct/union has no tag name
    }

    if (!hasName && (typeToken == PT_STRUCT)) {
        printError("Structure definition requires name");
    }

    acceptToken(TT_OPEN_BRACE);
    addNode(structBase, parse_struct_vars());
    acceptToken(TT_CLOSE_BRACE);
    return createListNode(structBase);
}

//---------------------------------------------------------------------
//   Statement parsing
//---------------------------------------------------------------------
//      for ( forexpr ) codeBlock
//      while ( condExpr ) codeBlock
//      if ( condExpr ) then codeBlock else codeBlock
//      return expr
//      function name ( arguments ) codeBlock
//      type varName = expression
//      postfix = expression
//
//   codeBlock:
//      statement ;
//      { statement* }

//-------------------------------------------
// for ( initStmt; condExpr; incStmt )
ListNode parse_stmt_for() {
    List *forStmt = createList(5);
    acceptToken(TT_FOR);
    acceptToken(TT_OPEN_PAREN);

    addNode(forStmt, createParseToken(PT_FOR));

    addNode(forStmt, parse_expr_assignment());
    acceptToken(TT_SEMICOLON);
    addNode(forStmt, parse_expr_logical());
    acceptToken(TT_SEMICOLON);
    addNode(forStmt, parse_expr_assignment());

    acceptToken(TT_CLOSE_PAREN);
    addNode(forStmt, parse_codeBlock());
    return createListNode(forStmt);
}

//-------------------------------------------
// loop (varname, startnum, cnt) {}
ListNode parse_stmt_loop() {
    List *loopNode = createList(5);
    acceptToken(TT_LOOP);
    acceptToken(TT_OPEN_PAREN);
    addNode(loopNode, createParseToken(PT_LOOP));

    addNode(loopNode, createStrNode(copyTokenStr(getToken())));
    acceptToken(TT_COMMA);
    addNode(loopNode, parse_expr());
    acceptToken(TT_COMMA);
    addNode(loopNode, parse_expr());

    acceptToken(TT_CLOSE_PAREN);
    addNode(loopNode, parse_codeBlock());
    return createListNode(loopNode);
}

ListNode parse_stmt_while() {
    List *whileStmt = createList(3);
    acceptToken(TT_WHILE);
    addNode(whileStmt, createParseToken(PT_WHILE));
    acceptToken(TT_OPEN_PAREN);
    addNode(whileStmt, parse_expr_logical());
    acceptToken(TT_CLOSE_PAREN);
    addNode(whileStmt, parse_codeBlock());
    return createListNode(whileStmt);
}

ListNode parse_stmt_do() {
    List *doStmt = createList(3);
    acceptToken(TT_DO);
    addNode(doStmt, createParseToken(PT_DOWHILE));
    addNode(doStmt, parse_codeBlock());
    acceptToken(TT_WHILE);
    acceptToken(TT_OPEN_PAREN);
    addNode(doStmt, parse_expr_logical());
    acceptToken(TT_CLOSE_PAREN);
    return createListNode(doStmt);
}


//-------------------------------------------
// Parse switch statement:
//    [switch, [expr], [case, value, [code]], [case, value, [code]], ..., [default, [code]] ]
ListNode parse_stmt_switch() {
    List *switchStmt = createList(MAX_SWITCH_CASES);
    acceptToken(TT_SWITCH);
    addNode(switchStmt, createParseToken(PT_SWITCH));
    acceptToken(TT_OPEN_PAREN);
    addNode(switchStmt, parse_expr());
    acceptToken(TT_CLOSE_PAREN);
    acceptToken(TT_OPEN_BRACE);
    bool switchError = false;
    while ((peekToken()->tokenType != TT_CLOSE_BRACE) && !switchError) {
        List *caseStmt = createList(3);

        TokenObject *token = peekToken();
        if (token->tokenType == TT_CASE) {
            acceptToken(TT_CASE);
            addNode(caseStmt, createParseToken(PT_CASE));
            ListNode caseVarNode = parse_primary_expr(false, false, false);
            if (acceptOptionalToken(TT_PERIOD)) {
                ListNode casePropRefNode = parse_identifier();

                List *propRef = createList(3);
                addNode(propRef, createParseToken(PT_PROPERTY_REF));
                addNode(propRef, caseVarNode);
                addNode(propRef, casePropRefNode);

                addNode(caseStmt, createListNode(propRef));
            } else {
                addNode(caseStmt, caseVarNode);
            }
            acceptToken(TT_COLON);
            addNode(caseStmt, parse_codeBlock());
        } else if (token->tokenType == TT_DEFAULT) {
            acceptToken(TT_DEFAULT);
            addNode(caseStmt, createParseToken(PT_DEFAULT));
            acceptToken(TT_COLON);
            addNode(caseStmt, parse_codeBlock());
        } else {
            switchError = true;
        }
        if (!switchError) {
            addNode(switchStmt, createListNode(caseStmt));
        }
    }
    if (switchError) {
        printError("Error parsing switch statement");
        while (peekToken()->tokenType != TT_CLOSE_BRACE);
    }
    acceptToken(TT_CLOSE_BRACE);

    switchStmt = condenseList(switchStmt);
    return createListNode(switchStmt);
}


ListNode parse_stmt_if() {
    List *ifStmt = createList(4);
    acceptToken(TT_IF);
    addNode(ifStmt, createParseToken(PT_IF));
    acceptToken(TT_OPEN_PAREN);
    addNode(ifStmt, parse_expr_logical());
    acceptToken(TT_CLOSE_PAREN);
    addNode(ifStmt, parse_codeBlock());
    if (peekToken()->tokenType == TT_ELSE) {
        acceptToken(TT_ELSE);
        addNode(ifStmt, parse_codeBlock());
    }
    return createListNode(ifStmt);
}

ListNode parse_stmt_return() {
    List *retStmt = createList(2);
    acceptToken(TT_RETURN);
    addNode(retStmt, createParseToken(PT_RETURN));
    if (peekToken()->tokenType != TT_SEMICOLON) {
        addNode(retStmt, parse_expr());
    } else {
        addNode(retStmt, createEmptyNode());
    }
    return createListNode(retStmt);
}

ListNode parse_strobe() {
    List *strobeStmt = createList(2);
    acceptToken(TT_STROBE);
    addNode(strobeStmt, createParseToken(PT_STROBE));
    addNode(strobeStmt, parse_expr());
    return createListNode(strobeStmt);
}

ListNode parse_stmt_break() {
    acceptToken(TT_BREAK);
    return createParseToken(PT_BREAK);
}

ListNode parse_stmt() {
    ListNode node = createEmptyNode();
    TokenObject *token = peekToken();

    switch (getTokenSymbolType(token)) {
        case TT_BREAK:  node = parse_stmt_break(); break;
        case TT_FOR:    node = parse_stmt_for(); break;
        case TT_LOOP:   node = parse_stmt_loop(); break;
        case TT_ASM:    node = parse_asmBlock(); break;
        case TT_DO:     node = parse_stmt_do();  break;
        case TT_WHILE:  node = parse_stmt_while(); break;
        case TT_IF:     node = parse_stmt_if();    break;
        case TT_SWITCH: node = parse_stmt_switch();  break;
        case TT_RETURN: node = parse_stmt_return();  break;
        case TT_STROBE: node = parse_strobe();      break;
        case TT_OPEN_BRACE: node = parse_codeBlock(); break;
        default:
            if (isTypeToken(token) || isModifierToken(token)) {
                // handle var list/initialization
                node = parse_variable();
            } else if (isIdentifier(token) && TypeList_find(token->tokenStr)) {
                // handle variables using user defined type
                node = parse_variable();
            } else if (token->tokenStr[0] == '#') {
                node = parse_compilerDirective(SCOPE_CODEBLOCK);
            } else if (token->tokenType != TT_SEMICOLON) {
                // handle an expression/assignment statement
                node = parse_expr_assignment();
            }
    }
    acceptOptionalToken(TT_SEMICOLON);
    return node;
}

ListNode parse_stmt_block(void) {
    List *list = createList(MAX_STATEMENTS_IN_BLOCK);   // TODO: maybe make list auto expand?
    addNode(list, createParseToken(PT_CODE));

    acceptToken(TT_OPEN_BRACE);
    while (hasToken() && (peekToken()->tokenType != TT_CLOSE_BRACE)) {
        bool canAdd = true;

        ListNode codeNode = parse_stmt();

        if (codeNode.type != N_EMPTY) {
            // process compound stmt - mainly for multiple vars declared on a single line (comma separated list)
            if (codeNode.type == N_LIST && codeNode.value.list->nodes[0].type == N_LIST) {
                List *subStmtList = codeNode.value.list;
                int cnt = 0;
                while (cnt < subStmtList->count && canAdd) {
                    canAdd = addNode(list, subStmtList->nodes[cnt++]);
                }
            } else {
                canAdd = addNode(list, codeNode);
            }
            if (!canAdd) {
                printError("Too many statements in current block");
                while (peekToken()->tokenType != TT_CLOSE_BRACE) getToken();
            }
        }
    }
    acceptToken(TT_CLOSE_BRACE);

    list = condenseList(list);
    return createListNode(list);
}

ListNode parse_codeBlock() {
    TokenObject *token = peekToken();
    if (token->tokenType == TT_ASM) {
        return parse_asmBlock();
    } else if (token->tokenType == TT_OPEN_BRACE) {
        return parse_stmt_block();
    } else {
        List *list = createList(2);
        addNode(list, createParseToken(PT_CODE));
        addNode(list, parse_stmt());
        return createListNode(list);
    }
}

ListNode parse_program(char *sourceCode, const char *srcName) {
    if (compilerOptions.showGeneralInfo) printf("Parsing %s...\n", srcName);
    parserErrorCount = 0;

    initTokenizer(sourceCode);

    ListNode progNode;

    List *prog = createList(MAX_PROG_DECLARATIONS);
    addNode(prog, createParseToken(PT_PROGRAM));

    char *tokenStr = (char *)allocMem(100);
    while (hasToken()) {
        ListNode node;

        // we only want to peek at next token...
        //   so that it's available to the chosen parse function

        // TODO: Maybe do this a different way?  TokenStr is probably duplicated to avoid
        //         print issues (when printing errors).

        tokenStr[0] = '\0';
        TokenObject *token = peekToken();
        if ((token != NULL) && (token->tokenType != TT_NONE)) {
            strncpy(tokenStr, token->tokenStr, 99);

            switch (getTokenSymbolType(token)) {
                case TT_ENUM:
                    node = parse_enum();
                    break;
                case TT_STRUCT:
                    node = parse_structOrUnion(PT_STRUCT);
                    break;
                case TT_UNION:
                    node = parse_structOrUnion(PT_UNION);
                    break;
                default:
                    if (isTypeToken(token) || isModifierToken(token)) {   // handle var list/initialization
                        node = parse_variable();
                    } else if (isIdentifier(token)) {
                        if (TypeList_find(token->tokenStr) != NULL) {   // handle user-defined type - var definition
                            node = parse_variable();
                        } else {
                            printError("Unknown type or unexpected identifier: %s\n", tokenStr);
                            getToken();
                        }
                    } else if (tokenStr[0] == '#') {
                        node = parse_compilerDirective(SCOPE_PROGRAM);
                    } else {
                        printError("Unexpected token: '%s'", tokenStr);
                        getToken();
                        node = createEmptyNode();
                    }
            }

            // eat the semicolons, they are optional
            acceptOptionalToken(TT_SEMICOLON);

            // process compound stmt - mainly for multiple vars declared on a single line (comma separated list)
            unwarpNodeList(prog, &node);
        }
    }
    free(tokenStr);

    killTokenizer();

    parserErrorCount = errorCount;
    prog = condenseList(prog);
    progNode = createListNode(prog);
    return progNode;
}
