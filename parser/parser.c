/*
 *   Neolithic Parser
 *
 *   Recursive Decent style parser
 *
 *   TODO: Fix issues with unary style operators.
 *          Unary ops should be parsed right before primitives.
 *          The AddrOf '&' already is.  It's currently done inside
 *          the primary expression routine (parenthesis / primitive)
 *          handling which is not quite the right place for it.
 *
 *   TODO: Fix negative/positive signs (part of the previous todo)
 *
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

//---------------------------
//  Global variables

int parserErrorCount;

/*-----------------------
// general functions
*/
const int MAX_PARSER_ERRORS = 3;
static int errorCount = 0;
static bool noMoreErrors = false;
void printError(const char* fmtErrorMsg, ...) {
    if (noMoreErrors) return;

    if (errorCount < MAX_PARSER_ERRORS) {
        char errorMsg[120];

        va_list argList;
        va_start(argList, fmtErrorMsg);
        vsnprintf(errorMsg, 120, fmtErrorMsg, argList);
        va_end(argList);

        printf("Error on line %d:  %s\n", getProgLineNum(), errorMsg);
        errorCount++;
    } else if (errorCount == MAX_PARSER_ERRORS) {
        printf("NOTE: Error limit exceeded.  No more errors will be reported.\n\n");
        noMoreErrors = true;
    }
}

int accept(const char *testStr) {
    char *token = getToken()->tokenStr;
    if (strncmp(token, testStr, 32) != 0) {
        printError("Unexpected token: '%s' -- was looking for: '%s'\n", token, testStr);
        return 0;
    }
    return 1;
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
        printf("Error: Identifier expected!\n");
    }
    return node;
}

ListNode parse_list() {
    // TODO: add support for parsing list
    while (peekToken()->tokenStr[0] != ']') {
        getToken();
    }
    accept("]");
    return createEmptyNode();
}

ListNode parse_hashmap() {
    // TODO: add support for parsing hashmaps and c-style lists

    List *items = createList(257);
    addNode(items, createParseToken(PT_LIST));

    while (peekToken()->tokenStr[0] != '}') {
        ListNode node = parse_expr();
        addNode(items, node);
        if (peekToken()->tokenStr[0] == ',') accept(",");
    }
    accept("}");
    return createListNode(items);
}

ListNode parse_cast_op() {
    List *castingList = createList(10);

    // capture all casting tokens
    while (peekToken()->tokenStr[0] != ')') {
        char *castToken = copyTokenStr(getToken());
        addNode(castingList, createStrNode(castToken));
    }
    accept(")");
    return createListNode(castingList);
}

ListNode parse_cast_expr() {
    ListNode node;
    List *castExprList = createList(3);
    addNode(castExprList, createParseToken(PT_CAST));
    addNode(castExprList, parse_cast_op());

    // if parenthesis follows, parse that expression
    if (peekToken()->tokenStr[0] == '(') {
        accept("(");
        addNode(castExprList, parse_expr());
        accept(")");
    } else {
        addNode(castExprList, parse_expr());
    }
    node = createListNode(castExprList);
    return node;
}

ListNode parse_nested_primary(int allowNestedExpr, const char *tokenStr) {
    ListNode node;
    switch (tokenStr[0]) {
        case '(':
            if (allowNestedExpr) {
                //printf("found parenthetical expression\n");
                TokenObject *ptoken = peekToken();
                if ((isTypeToken(ptoken) || isModifierToken(ptoken))) {
                    node = parse_cast_expr();
                } else {
                    node = parse_expr();
                    accept(")");
                }
            } else {
                printError("nested expression not allowed here\n");
                node = createEmptyNode();
            }
            break;
        case '[':
            // found list
            node = parse_list();
            break;
        case '{':
            // found hashmap
            node = parse_hashmap();
            break;
        default:
            printError("Invalid symbol: %s,  expected primitive\n", tokenStr);
            node = createEmptyNode();
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

    switch(exprType) {
        case TT_NUMBER:      /* numeric const */
            if (!isLValue) {
                node = createIntNode(copyTokenInt(token));
            } else {
                printError("Improper start of statement: \"%s\"... must be an identifier\n", tokenStr);
            }
            break;
        case TT_IDENTIFIER:
            identifier = copyTokenStr(token);
            node = createStrNode(identifier);
            break;
        case TT_STRING:
            //node = createListNode(createStrConst());
            printError("found string const: %s\n", tokenStr);
            break;
        case TT_SYMBOL:
            if (tokenStr[0] == '&') {
                List *list = createList(2);
                addNode(list, createParseToken(PT_ADDR_OF));
                addNode(list, parse_primary_expr(isLValue, isExprAllowed, allowNestedExpr));
                node = createListNode(list);
            } else if ((tokenStr[0] == '-') && (peekToken()->tokenType == TT_NUMBER)) {
                node = createIntNode(-copyTokenInt(getToken()));
            } else if (isExprAllowed) {
                node = parse_nested_primary(allowNestedExpr, tokenStr);
            } else {
                printError("Expression not allowed here");
            }
            break;
        default:
            printError("Primitive not found....found token '%s' instead\n", tokenStr);
    }
    return node;
}

ListNode parse_arguments() {
    if (!(inCharset(peekToken(), ")"))) {
        List *list = createList(20);
        addNode(list, parse_expr());

        while (inCharset(peekToken(), ",")) {
            accept(",");
            addNode(list, parse_expr());
        }
        return createListNode(list);
    } else {
        return createEmptyNode();
    }
}

//--------------------------------------------------------------------
//   Parse postfix expressions:
//       primitive
//       postfixExpr [ expr ]           -- array/object lookup
//       postfixExpr ( argumentList )   -- function call
//       postfixExpr . identifier       -- reference object property
//       postfixExpr ++ / --

ListNode parse_expr_postfix(bool isLValue, int allowNestedExpr) {
    ListNode lnode, rnode, opNode;
    List *list = NULL;
    char tokenChar;

    lnode = parse_primary_expr(isLValue, true, allowNestedExpr);
    while (inCharset(peekToken(), "[(.")) {
        tokenChar = getToken()->tokenStr[0];

        // handle right node depending on which path we're taking;
        switch (tokenChar) {
            case '[':
                if (!inCharset(peekToken(), "]")) {
                    rnode = parse_expr();
                    accept("]");
                    opNode = createParseToken(PT_LOOKUP);
                } else {
                    // TODO: this is only useful for var definitions
                    accept("]");
                    opNode = createParseToken(PT_ARRAY);
                    rnode = createEmptyNode();
                }
                break;
            case '(':
                rnode = parse_arguments();
                accept(")");
                opNode = createParseToken(PT_FUNC_CALL);
                break;
            case '.':
                rnode = parse_identifier();
                opNode = createParseToken(PT_PROPERTY_REF);
                break;
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

    if (inCharset(peekToken(), "!+*~")) {
        tokenChar = getToken()->tokenStr[0];
        switch (tokenChar) {
            //case '&': opNode = createParseToken(PT_ADDR_OF); break;
            case '!': opNode = createParseToken(PT_NOT); break;
            case '+': opNode = createParseToken(PT_POSITIVE); break;
            //
            // case '-': opNode = createParseToken(PT_NEGATIVE); break;
            case '~': opNode = createParseToken(PT_INVERT); break;
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
    char tokenChar;

    lnode = parse_expr_unary();
    while (inCharset(peekToken(), "*/")) {
        tokenChar = getToken()->tokenStr[0];
        if (tokenChar == '*') opNode = createParseToken(PT_MULTIPLY);
        else if (tokenChar == '/') opNode = createParseToken(PT_DIVIDE);
        else opNode = createCharNode(tokenChar);
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
    char tokenChar;

    lnode = parse_expr_mulDiv();
    while (inCharset(peekToken(), "+-")) {
        tokenChar = getToken()->tokenStr[0];
        if (tokenChar == '+') opNode = createParseToken(PT_ADD);
        else if (tokenChar == '-') opNode = createParseToken(PT_SUB);
        else opNode = createCharNode(tokenChar);
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
                opNode = createStrNode(lookupTokenSymbol(token));
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
    if (inCharset(peekToken(), "?")) {
        accept("?");
        opNode = createStrNode("ifExpr");
        mnode = parse_expr_logical();
        accept(":");
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
    TokenType specialAsgnType;
    ListNode lnode, rnode, opNode;
    List *list;

    lnode = parse_expr_postfix(true, 0);  // parse lvalue

    // check if assignment OR just a definition or function call
    bool isBasicAsgnEqual = inCharset(peekToken(), "=");
    bool isAsgnOpEqual = isAddSubAsgn(peekToken()->tokenType);

    if (isBasicAsgnEqual || isAsgnOpEqual) {

        list = createList(3);

        if (isBasicAsgnEqual) {
            accept("=");
            opNode = createParseToken(PT_SET);
            specialAsgnType = TT_EQUAL;
        } else {
            //--- Handle special assignment:  += -=
            //--- need to save the tokenType since token buffer will get overwritten
            //---   when rnode is processed

            TokenObject *asgnToken = getToken();
            specialAsgnType = asgnToken->tokenType;
            opNode = createStrNode(lookupTokenSymbol(asgnToken));
        }
        rnode = parse_expr();

        /* for add_to/sub_from, need to build sub expression*/
        if (isAsgnOpEqual) {
            List *innerList = createList(3);
            switch (specialAsgnType) {
                case TT_ADD_TO:
                    addNode(innerList, createParseToken(PT_ADD));
                    break;
                case TT_SUB_FROM:
                    addNode(innerList, createParseToken(PT_SUB));
                    break;
                default:
                    addNode(innerList, opNode);
            }
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

bool isVarAsgnNext() {
    return inCharset(peekToken(), "=");
}

ListNode parse_var_assignment() {
    List *list = createList(3);

    accept("=");
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
    ListNode arrayNode = createEmptyNode();
    if (inCharset(peekToken(), "[")) {
        accept("[");
        if (!inCharset(peekToken(), "]")) {
            TokenObject *token = getToken();
            if (token->tokenType == TT_NUMBER) {
                int arraySize = copyTokenInt(token);
                arrayNode = createIntNode(arraySize);
            } else {
                arrayNode = createStrNode(copyTokenStr(token));
            }
        } else {
            arrayNode = createParseToken(PT_ARRAY);
        }
        accept("]");
    }
    return arrayNode;
}

ListNode parse_var_node(const char *baseType, const List *modList, const char *regHint) {
    bool isFunctionDef = false;
    ListNode varNameNode, asgnNode, paramNode;
    List *list = createList(6);
    List *typeList = createList(3);

    addNode(typeList, createStrNode(baseType));

    // handle memory hint
    int memAddr = 0;
    bool hasMemHint = false;
    if (inCharset(peekToken(), "@")) {
        accept("@");
        memAddr = copyTokenInt(getToken());
        hasMemHint = true;
    }

    // handle pointer reference
    if (inCharset(peekToken(), "*")) {
        accept("*");
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
        addNode(regHintList, createStrNode(strdup(regHint)));
        addNode(typeList, createListNode(regHintList));
    }

    // handle function declarator
    if (peekToken()->tokenStr[0] == '(') {
        paramNode = parse_func_parameters();
        isFunctionDef = true;
    } else {
        paramNode = createEmptyNode();
    }

    // handle initializer
    asgnNode = (isVarAsgnNext() ? parse_var_assignment() : createEmptyNode());

    // build a variable node
    //list = createList(6);

    addNode(list, createParseToken(isFunctionDef ? PT_DEFUN : PT_DEFINE));
    addNode(list, varNameNode);
    addNode(list, createListNode(typeList));
    addNode(list, createListNode((List *) modList));
    addNode(list, isFunctionDef ? paramNode : asgnNode);
    if (hasMemHint) {
        addNode(list, createIntNode(memAddr));
    }

    if (isFunctionDef && (peekToken()->tokenStr[0] == '{'
                            || (strncmp(peekToken()->tokenStr, "asm", 3) == 0)) ) {
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
        switch (findTokenSymbol(modToken->tokenStr)->tokenType) {
            case TT_CONST:    parseToken = PT_CONST;    break;
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
    return modList;
}

ListNode parse_variable(void) {
    List *modList = parse_mod_list();
    char *baseType = copyTokenStr(getToken());

    // parse first variable
    ListNode varNode = parse_var_node(baseType, modList, NULL);

    //----- at this point, we can have multiple parameters separated by commas
    if (inCharset(peekToken(), ",")) {
        List *varList = createList(20);
        addNode(varList, varNode);
        do {
            accept(",");

            varNode = parse_var_node(baseType, modList, NULL);
            addNode(varList, varNode);

        } while (inCharset(peekToken(), ","));
        return createListNode(varList);
    } else {
        return varNode;
    }
}

ListNode parse_parameter() {
    if (isTypeToken(peekToken()) || isModifierToken(peekToken())) {
        // handle mod list
        List *modList = parse_mod_list();

        char *baseType = copyTokenStr(getToken());
        char *regHint = NULL;
        if (peekToken()->tokenStr[0] == '@') {
            // it's a register hint
            accept("@");
            regHint = copyTokenStr(getToken());
        }
        return parse_var_node(baseType, modList, regHint);
    } else {
        char *varName = copyTokenStr(getToken());
        return createStrNode(varName);
    }
}

ListNode parse_parameters() {
    List *list = createList(20);
    addNode(list, createParseToken(PT_PARAMLIST));
    addNode(list, parse_parameter());

    while (inCharset(peekToken(), ",")) {
        accept(",");
        addNode(list, parse_parameter());
    }

    return createListNode(list);
}

ListNode parse_func_parameters(void) {
    ListNode funcParams;
    accept("(");
    char *tokenStr = peekToken()->tokenStr;
    bool isVoid = (strncmp(tokenStr, "void", 4) == 0);
    if (!isVoid && tokenStr[0] != ')') {
        funcParams = parse_parameters();
    } else {
        if (isVoid) accept("void");
        funcParams = createEmptyNode();
    }
    accept(")");
    return funcParams;
}

//-------------------------------------
//   Enumeration parsing
//-------------------------------------
//    enum EnumTag { var = value, ... }

ListNode parse_enum() {
    int currentEnumValue = 0;
    List *enumList = createList(260);
    accept("enum");
    addNode(enumList, createParseToken(PT_ENUM));

    char *enumName = copyTokenStr(getToken());
    addNode(enumList, createStrNode(enumName));  // enum name
    TypeList_add(enumName);

    accept("{");
    while (!inCharset(peekToken(), "}")) {
        List *enumNode = createList(2);
        char *valueName = copyTokenStr(getToken());

        // did user set enumeration value?
        if (peekToken()->tokenStr[0] == '=') {
            accept("=");
            currentEnumValue = copyTokenInt(getToken());
        }

        // build the node
        addNode(enumNode, createStrNode(valueName));
        addNode(enumNode, createIntNode(currentEnumValue));
        addNode(enumList, createListNode(enumNode));
        currentEnumValue++;

        if (peekToken()->tokenStr[0] == ',') accept(",");
    }
    accept("}");
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

#define STRUCT_VAR_LIMIT 200

ListNode parse_structOrUnion();     // Forward Declaration

ListNode parse_struct_vars() {
    List *varList = createList(STRUCT_VAR_LIMIT);
    ListNode node;

    addNode(varList, createParseToken(PT_VARS));

    while (!inCharset(peekToken(), "}")) {
        char *token = peekToken()->tokenStr;
        if ((strcmp(token, "struct") == 0) || (strcmp(token, "union") == 0)) {
            node = parse_structOrUnion();
        } else {
            node = parse_variable();
        }
        addNode(varList, node);
        if (peekToken()->tokenStr[0] == ';') accept(";");
    }
    return createListNode(varList);
}

ListNode parse_structOrUnion() {
    List *structBase = createList(3);

    // union or struct indicator
    if (peekToken()->tokenStr[0] == 's') {
        accept("struct");
        addNode(structBase, createParseToken(PT_STRUCT));
    } else {
        accept("union");
        addNode(structBase, createParseToken(PT_UNION));
    }

    if (peekToken()->tokenStr[0] != '{') {
        char *typeName = copyTokenStr(getToken());
        TypeList_add(typeName);
        addNode(structBase, createStrNode(typeName));
    } else {
        addNode(structBase, createEmptyNode()); // struct/union has no tag name
    }
    accept("{");
    addNode(structBase, parse_struct_vars());
    accept("}");
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
// for ( type varName in expr )
ListNode parse_stmt_for() {
    List *forStmt = createList(5);
    accept("for");
    addNode(forStmt, createParseToken(PT_FOR));

    if (!accept("(")) return createEmptyNode();

    if (!(inCharset(peekToken(),";)"))) {
        addNode(forStmt, parse_expr_assignment());
    } else {
        addNode(forStmt, createEmptyNode());
    }
    if (!accept(";")) return createEmptyNode();

    if (!(inCharset(peekToken(),";)"))) {
        // parse loop comparison
        addNode(forStmt, parse_expr_logical());
    } else {
        addNode(forStmt, createEmptyNode());
    }
    if (!accept(";")) return createEmptyNode();

    if (!(inCharset(peekToken(),")"))) {
        // parse looped expression
        addNode(forStmt, parse_expr_assignment());
    } else {
        addNode(forStmt, createEmptyNode());
    }
    accept(")");
    addNode(forStmt, parse_codeBlock());
    return createListNode(forStmt);
}

ListNode parse_stmt_while() {
    List *whileStmt = createList(3);
    accept("while");
    addNode(whileStmt, createParseToken(PT_WHILE));
    accept("(");
    addNode(whileStmt, parse_expr_logical());
    accept(")");
    addNode(whileStmt, parse_codeBlock());
    return createListNode(whileStmt);
}

ListNode parse_stmt_do() {
    List *doStmt = createList(3);
    accept("do");
    addNode(doStmt, createParseToken(PT_DOWHILE));
    addNode(doStmt, parse_codeBlock());
    accept("while");
    accept("(");
    addNode(doStmt, parse_expr_logical());
    accept(")");
    return createListNode(doStmt);
}


//-------------------------------------------
// Parse switch statement:
//    [switch, [expr], [case, value, [code]], [case, value, [code]], ..., [default, [code]] ]
ListNode parse_stmt_switch() {
    List *switchStmt = createList(20);
    accept("switch");
    addNode(switchStmt, createParseToken(PT_SWITCH));
    accept("(");
    addNode(switchStmt, parse_expr());
    accept(")");
    accept("{");
    bool switchError = false;
    while ((peekToken()->tokenStr[0] != '}') && !switchError) {
        List *caseStmt = createList(3);
        if (strcmp(peekToken()->tokenStr, "case") == 0) {
            accept("case");
            addNode(caseStmt, createParseToken(PT_CASE));
            addNode(caseStmt, parse_primary_expr(false, false, false));
            accept(":");
            addNode(caseStmt, parse_codeBlock());
        } else if (strcmp(peekToken()->tokenStr, "default") == 0) {
            accept("default");
            addNode(caseStmt, createParseToken(PT_DEFAULT));
            accept(":");
            addNode(caseStmt, parse_codeBlock());
        } else switchError = true;
        addNode(switchStmt, createListNode(caseStmt));
    }
    if (switchError) {
        printError("Error parsing switch statement");
    }
    accept("}");
    return createListNode(switchStmt);
}


ListNode parse_stmt_if() {
    List *ifStmt = createList(4);
    accept("if");
    addNode(ifStmt, createParseToken(PT_IF));
    accept("(");
    addNode(ifStmt, parse_expr_logical());
    accept(")");
    addNode(ifStmt, parse_codeBlock());
    if (!strncmp(peekToken()->tokenStr, "else", 4)) {
        accept("else");
        addNode(ifStmt, parse_codeBlock());
    }
    return createListNode(ifStmt);
}

ListNode parse_stmt_return() {
    List *retStmt = createList(2);
    accept("return");
    addNode(retStmt, createParseToken(PT_RETURN));
    addNode(retStmt, parse_expr());
    return createListNode(retStmt);
}

ListNode parse_strobe() {
    List *strobeStmt = createList(2);
    accept("strobe");
    addNode(strobeStmt, createParseToken(PT_STROBE));
    addNode(strobeStmt, parse_expr());
    return createListNode(strobeStmt);
}

ListNode parse_stmt() {
    ListNode node = createEmptyNode();
    TokenObject *token = peekToken();

    switch (token->tokenSymbol->tokenType) {
        case TT_FOR:    node = parse_stmt_for(); break;
        case TT_ASM:    node = parse_asmBlock(); break;
        case TT_DO:     node = parse_stmt_do();  break;
        case TT_WHILE:  node = parse_stmt_while(); break;
        case TT_IF:     node = parse_stmt_if();    break;
        case TT_SWITCH: node = parse_stmt_switch();  break;
        case TT_RETURN: node = parse_stmt_return();  break;
        case TT_STROBE: node = parse_strobe();      break;
        default:
            if (isTypeToken(token) || isModifierToken(token)) {
                // handle var list/initialization
                node = parse_variable();
            } else if (!(inCharset(token, ";"))) {
                // handle an expression/assignment statement
                node = parse_expr_assignment();
            }
    }
    if (peekToken()->tokenStr[0] == ';') accept(";");
    return node;
}

ListNode parse_stmt_block(void) {
    List *list = createList(300);   // TODO: need to make list auto expand!!!
    addNode(list, createParseToken(PT_CODE));

    accept("{");
    while (!(inCharset(peekToken(), "}"))) {
        ListNode codeNode = parse_stmt();

        // process compound stmt - mainly for multiple vars declared on a single line (comma separated list)
        if (codeNode.type == N_LIST && codeNode.value.list->nodes[0].type == N_LIST) {
            List *subStmtList = codeNode.value.list;
            for (int cnt=0; cnt<subStmtList->count; cnt++) {
                addNode(list, subStmtList->nodes[cnt]);
            }
        } else {
            addNode(list, codeNode);
        }
    }
    accept("}");

    return createListNode(list);
}

ListNode parse_codeBlock() {
    if (strncmp(peekToken()->tokenStr, "asm", 3) == 0) {
        return parse_asmBlock();
    } else if (inCharset(peekToken(), "{")) {
        return parse_stmt_block();
    } else {
        List *list = createList(2);
        addNode(list, createParseToken(PT_CODE));
        addNode(list, parse_stmt());
        return createListNode(list);
    }
}

ListNode parse_program(char *sourceCode) {
    printf("Parsing...\n");
    parserErrorCount = 0;

    initTokenizer(sourceCode);

    ListNode progNode;

    List *prog = createList(100);
    addNode(prog, createParseToken(PT_PROGRAM));

    char *tokenStr = (char *)malloc(100);
    while (hasToken()) {
        ListNode node;

        // we only want to peek at next token...
        //   so that it's available to the chosen parse function
        tokenStr[0] = '\0';
        TokenObject *token = peekToken();
        if ((token != NULL) && (token->tokenType != TT_NONE)) {
            strncpy(tokenStr, token->tokenStr, 99);
        }
        switch (token->tokenSymbol->tokenType) {
            case TT_ENUM:     node = parse_enum(); break;
            case TT_STRUCT:   node = parse_structOrUnion(); break;
            case TT_UNION:    node = parse_structOrUnion(); break;
            default:
                if (isTypeToken(token) || isModifierToken(token)) {   // handle var list/initialization
                    node = parse_variable();
                } else if (isIdentifier(token)) {
                    if (TypeList_find(token->tokenStr) != NULL) {   // handle user-defined type - var definition
                        node = parse_variable();
                    } else {                                        // handle an expression/assignment statement
                        //printf("Assignment: %s\n", tokenStr);
                        //node = parse_expr_assignment();
                        printError("Warning: Unknown type or unexpected identifier: %s\n", tokenStr);
                        getToken();
                    }
                } else {
                    // handle case where no token came back: blank line/comments
                    if (tokenStr[0] != '\0' && tokenStr[0] != '\r' && tokenStr[0] != '\n') {  // ignore null strings
                        printError("Warning: bad token: %s %d\n", tokenStr, tokenStr[0]);
                    }
                    getToken();
                    node = createEmptyNode();
                }
        }

        if (peekToken()->tokenStr[0] == ';') accept(";");
        if (node.type != N_EMPTY) {
            // process compound stmt - mainly for multiple vars declared on a single line (comma separated list)
            if (isListNode(node)) {
                List *subStmtList = node.value.list;
                for (int cnt=0; cnt<subStmtList->count; cnt++) {
                    addNode(prog, subStmtList->nodes[cnt]);
                }
            } else {
                // add normal node to program (for most cases)
                addNode(prog, node);
            }
        }
    }
    free(tokenStr);

    killTokenizer();

    parserErrorCount = errorCount;
    progNode = createListNode(prog);
    return progNode;
}
