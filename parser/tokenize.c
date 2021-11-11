/*
 *    Tokenizer
 *
 *    TODO: While the tokenizer works well, it could probably be
 *          cleaned-up and simplified.  Parse tokens and Token Types
 *          could potentially be merged.  The removal of whitespace and
 *          comments, as well as, the detection of new lines could be
 *          simplified; mainly, all the small functions could be merged
 *          into a single function.
 *
 *          Overall, the code seems overly complicated and could probably
 *          be simplified.
 *
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "tokenize.h"
#include "data/identifiers.h"

/*------------------------------------------
     Local Variables
*/
static TokenObject *tokenStorage;
static TokenObject *currentToken;       // either tokenStorage or one of the builtin tokens
static char *tokenStr;
static int tokenStrLen;
static int tokenIndex;
static bool isFirstTokenOnLine;

// keep track of program line info for error messages (currently ONLY for the tokenizer/parser)
static char *progLineStart;     // start in file buffer for current program line
static int progLineNum;         // line number of current program line

static const TokenObject TokenSymbols[] = {
        {"",   TT_NONE,         TF_NONE},

        // single symbols
        {"#",  TT_HASH,         TF_OP},
        {"&",  TT_AMPERSAND,    TF_OP},
        {"(",  TT_OPEN_PAREN,   TF_OP},
        {")",  TT_CLOSE_PAREN,  TF_OP},
        {"*",  TT_MULTIPLY,     TF_OP},
        {"+",  TT_PLUS,         TF_OP},
        {",",  TT_COMMA,        TF_OP},
        {"-",  TT_MINUS,        TF_OP},
        {".",  TT_PERIOD,       TF_OP},
        {"/",  TT_DIVIDE,       TF_OP},
        {":",  TT_COLON,        TF_OP},
        {";",  TT_SEMICOLON,    TF_OP},
        {"<",  TT_LESS_THAN,    TF_OP},
        {"=",  TT_ASSIGN,       TF_OP},
        {">",  TT_GREATER_THAN, TF_OP},
        {"?",  TT_QUESTION,     TF_OP},
        {"@",  TT_AT_SIGN,      TF_OP},

        // NOTE:  brackets then braces
        {"[",  TT_OPEN_BRACKET,  TF_OP},
        {"]",  TT_CLOSE_BRACKET, TF_OP},
        {"{",  TT_OPEN_BRACE,    TF_OP},
        {"}",  TT_CLOSE_BRACE,   TF_OP},

        // double symbols
        {"==", TT_EQUAL,            TF_OP},
        {"!=", TT_NOT_EQUAL,        TF_OP},
        {">=", TT_GREATER_EQUAL,    TF_OP},
        {"<=", TT_LESS_EQUAL,       TF_OP},
        {"+=", TT_ADD_TO,           TF_OP},
        {"-=", TT_SUB_FROM,         TF_OP},
        {">>", TT_SHIFT_RIGHT,      TF_OP},
        {"<<", TT_SHIFT_LEFT,       TF_OP},
        {"&&", TT_BOOL_AND,         TF_OP},
        {"||", TT_BOOL_OR,          TF_OP},
        {"++", TT_INC,              TF_OP},
        {"--", TT_DEC,              TF_OP},

        //reserved words
        {"asm",     TT_ASM,         TF_OP},
        {"alias",   TT_ALIAS,       TF_MODIFIER},
        {"bool",    TT_BOOLEAN,     TF_TYPE},
        {"boolean", TT_BOOLEAN,     TF_TYPE},
        {"break",   TT_BREAK,       TF_OP},
        {"byte",    TT_BYTE,        TF_TYPE},
        {"case",    TT_CASE,        TF_OP},
        {"char",    TT_CHAR,        TF_TYPE},
        {"const",   TT_CONST,       TF_MODIFIER},
        {"default", TT_DEFAULT,     TF_OP},
        {"delete",  TT_DELETE,      TF_OP},
        {"do",      TT_DO,          TF_OP},
        {"else",    TT_ELSE,        TF_OP},
        {"enum",    TT_ENUM,        TF_OP},
        {"extern",  TT_EXTERN,      TF_MODIFIER},
        {"false",   TT_FALSE,       TF_OP},
        {"for",     TT_FOR,         TF_OP},
        {"function",TT_FUNCTION,    TF_OP},
        {"if",      TT_IF,          TF_OP},
        {"import",  TT_IMPORT,      TF_OP},
        {"in",      TT_IN,          TF_OP},
        {"inline",  TT_INLINE,      TF_MODIFIER},
        {"int",     TT_INT,         TF_TYPE},
        {"new",     TT_NEW,         TF_OP},
        {"null",    TT_NULL,        TF_OP},
        {"register",TT_REGISTER,    TF_MODIFIER},
        {"return",  TT_RETURN,      TF_OP},
        {"signed",  TT_SIGNED,      TF_MODIFIER},
        {"sizeof",  TT_SIZEOF,      TF_OP},
        {"static",  TT_STATIC,      TF_MODIFIER},
        {"strobe",  TT_STROBE,      TF_OP},
        {"struct",  TT_STRUCT,      TF_MODIFIER},
        {"switch",  TT_SWITCH,      TF_OP},
        {"true",    TT_TRUE,        TF_OP},
        {"type",    TT_TYPE,        TF_OP},
        {"typeof",  TT_TYPEOF,      TF_OP},
        {"union",   TT_UNION,       TF_OP},
        {"unsigned",TT_UNSIGNED,    TF_MODIFIER},
        {"var",     TT_VAR,         TF_OP},
        {"void",    TT_VOID,        TF_TYPE},
        {"while",   TT_WHILE,       TF_OP},
        {"word",    TT_WORD,        TF_TYPE},
        {"zeropage",TT_ZEROPAGE,    TF_MODIFIER},
};

static const int NumTokenSymbols = sizeof(TokenSymbols) / sizeof(TokenObject);


/* ------------------------------------------
 *    Internal Function declarations
 *
 */

void trimWhitespaceAndComments();
TokenType getDoubleSymbolTokenType(char *token, char firstChar, int tokenEnd);

/* ------------------------------------------
        Functions
 */

TokenObject * findTokenSymbol(char *tokenName) {
    int index;
    for (index=1; index < NumTokenSymbols; index++) {
        if (strncmp(tokenName, TokenSymbols[index].tokenStr, TOKEN_LENGTH_LIMIT) == 0) break;
    }
    if (index >= NumTokenSymbols) index = 0;
    return (TokenObject *)&TokenSymbols[index];
}

const char *lookupTokenSymbol(TokenType tokenType) {
    int i=0;
    do {
        if (TokenSymbols[i].tokenType == tokenType)
            return TokenSymbols[i].tokenStr;
    } while (++i < NumTokenSymbols);
    return "";
}

void initTokenizer(char *theInputStr) {
    tokenStr = theInputStr;
    tokenIndex = 0;
    progLineNum = 1;

    /* find tokenStr length */
	tokenStrLen = 0;
	while (tokenStr[tokenStrLen] != '\0') tokenStrLen++;

    // alloc memory for currentToken
	tokenStorage = (TokenObject *)allocMem(TOKEN_LINE_LIMIT);
	currentToken = tokenStorage;

    isFirstTokenOnLine = true;
}

void killTokenizer() {
    /* dealloc currentToken */
    free(currentToken);
}

int getProgLineNum() {
    return progLineNum;
}


/**
 * Get the current program line for use in the ASM output
 * @return program line (code) preceded by line number
 */
SourceCodeLine getProgramLineString() {
    int col = 0;

    if (progLineStart && progLineStart[0] != '\0') {

        if (progLineStart[0] == '\n') progLineStart++;  // skip first char if newline

        while (progLineStart[col] != '\n' && col < (TOKEN_LINE_LIMIT - 2)) col++;

    }

    // alright, we have all the information, package it up nicely for returning
    SourceCodeLine progLine;
    progLine.len = col;
    progLine.data = progLineStart;
    progLine.lineNum = progLineNum;
    return progLine;
}


/* figure out what token is by first character */
TokenType getTokenTypeByChar(char firstChar, char secondChar) {
    TokenType tokenType = TT_SYMBOL;
	if (isalpha(firstChar) || firstChar == '_') {
        tokenType = TT_IDENTIFIER;
    } else if (isdigit(firstChar) || firstChar == '$') {
        tokenType = TT_NUMBER;
    } else if (firstChar == '%' && isdigit(secondChar)) {   // allow DASM-style binary
	    tokenType = TT_NUMBER;
    } else if (firstChar == '\'' || firstChar == '"') {
        tokenType = TT_STRING;
    }
    return tokenType;
}

bool hasToken() {
    return (tokenIndex < tokenStrLen) && (tokenStr[tokenIndex] != '\0');
}

bool charInCharset(char ch, const char *charSet) {
    bool result = false;
    while ((*charSet != '\0') && !result) {
        if (ch == *charSet) result = true;
        charSet++;
    }
    return result;
}

bool inCharset(TokenObject *token, const char *charSet) {
    if (token->tokenStr[1] != '\0') return false;   // exit if token contains more than one character
    return charInCharset(token->tokenStr[0], charSet);
}

// reset current token
void resetToken() {
    currentToken = tokenStorage;
    currentToken->tokenType = TT_NONE;
    currentToken->tokenStr[0] = '\0';
    currentToken->tokenPos = tokenIndex;
}

TokenObject *parseToken() {
    char *token;
    bool tokenDone = false;
    TokenType tokenType;
    char firstChar, secondChar;

    int tokenEnd = 0;

    resetToken();

    // if we're out of tokens, return with blank token
    if (!hasToken()) return currentToken;

    /* link token string to currentToken object */
    token = currentToken->tokenStr;

    /*--------------------------------------------------*/
	/* figure out what token type is by first character */

	firstChar = tokenStr[tokenIndex++];
	token[tokenEnd++] = firstChar;
	secondChar = (tokenIndex < tokenStrLen) ? tokenStr[tokenIndex] : 0;
	tokenType = getTokenTypeByChar(firstChar, secondChar);
    currentToken->tokenType = tokenType;

	/* now grab the remainder characters in the token */
    switch (tokenType) {
        case TT_IDENTIFIER:
        case TT_NUMBER:
            while ((tokenIndex <= tokenStrLen) &&
                    (isalnum(tokenStr[tokenIndex]) || tokenStr[tokenIndex] == '_')) {
                token[tokenEnd++] = tokenStr[tokenIndex++];
            }
            break;
        case TT_STRING:
            while ((!tokenDone) && (tokenIndex <= tokenStrLen)) {
                if (tokenStr[tokenIndex] == firstChar)
                    tokenDone = true;
                token[tokenEnd++] = tokenStr[tokenIndex++];
            }
            break;
        case TT_SYMBOL:
            {
                // double symbols:      (2 groups)
                //    == != <= >= += -=   all end in equal char
                //    >> << && || ++ --    both chars are the same
                TokenType tokenType1 = getDoubleSymbolTokenType(token, firstChar, tokenEnd);
                if (tokenType1 != TT_SYMBOL) {
                    currentToken->tokenType = tokenType1;
                    tokenIndex++;
                    tokenEnd++;
                } else {
                    // process single symbols
                    switch (firstChar) {
                        case '<': currentToken->tokenType = TT_LESS_THAN;    break;
                        case '>': currentToken->tokenType = TT_GREATER_THAN; break;
                        default: break;
                    }
                }
            }
            break;
        default: break;
	}
    token[tokenEnd] = '\0';

    // If we have a token, do a lookup for reserved words
    if (tokenEnd > 0) {
        TokenObject *foundToken = findTokenSymbol(token);
        if (foundToken && foundToken->tokenType != TT_NONE) {
            currentToken = foundToken;
        }
        isFirstTokenOnLine = false;
    }
    return currentToken;
}

TokenObject *getToken() {
    trimWhitespaceAndComments();
    TokenObject *token = parseToken();
    return token;
}


//  TODO:  Create a simple token buffer to store the a couple of tokens.
//         This would help eliminate the need for the following trickiness.

TokenObject *peekToken() {
    trimWhitespaceAndComments();

    int savedIndex = tokenIndex;
    TokenObject *token = parseToken();
    tokenIndex = savedIndex;
    return token;
}


TokenType getTokenSymbolType(TokenObject *token) {
    return token->tokenType;
}

//------------------------------------------------------------------
//--  functions to copy token into appropriate data type for use

char * copyTokenStr(TokenObject *token) {
    assert(token != NULL);
    return Ident_add(token->tokenStr, NULL);
}

int copyTokenInt(TokenObject *token) {
    assert(token != NULL);
    return strToInt(token->tokenStr);
}

//---------------------------------------------------------
//  Simple token test/compare functions

bool isModifierToken(TokenObject *token) {
    return token && (token->tokenFlags == TF_MODIFIER);
}

bool isTypeToken(TokenObject *token) {
    return token && (token->tokenFlags == TF_TYPE);
}

bool isIdentifier(TokenObject *token) {
    return (token && (token->tokenType == TT_IDENTIFIER));
}

bool isEquality(TokenType symType) {
    return ((symType == TT_EQUAL) ||
            (symType == TT_NOT_EQUAL) ||
            (symType == TT_GREATER_THAN) ||
            (symType == TT_LESS_THAN) ||
            (symType == TT_GREATER_EQUAL) ||
            (symType == TT_LESS_EQUAL));
}
bool isOpAsgn(enum TokenType symType) {
    return ((symType == TT_ADD_TO) ||
            (symType == TT_SUB_FROM) ||
            (symType == TT_OR_WITH) ||
            (symType == TT_AND_WITH));
}

bool isShift(enum TokenType symType) {
    return ((symType == TT_SHIFT_LEFT) || (symType == TT_SHIFT_RIGHT));
}

bool isBoolAndOr(enum TokenType symType) {
    return ((symType == TT_BOOL_AND) || (symType == TT_BOOL_OR));
}

bool isIncDec(enum TokenType symType) {
    return ((symType == TT_INC) || (symType == TT_DEC));
}

bool isComparison(TokenObject *token) {
    if (token->tokenType != TT_SYMBOL) {
        return isEquality(token->tokenType);
    } else {
        return inCharset(token, "<>");
    }
}


//========================================================================
//  Internal functions

bool isStartOfComment() {
    return (tokenStr[tokenIndex] == '/' && tokenStr[tokenIndex+1] == '*');
}

bool isStartOfLineComment() {
    return (tokenStr[tokenIndex] == '/' && tokenStr[tokenIndex+1] == '/');/* ||
           (tokenStr[tokenIndex] == '#' && isFirstTokenOnLine);*/
}

bool isEol(char c) {
    return (c == '\n') || (c == '\r');
}
bool isWhiteSpace() {
    return isspace(tokenStr[tokenIndex]);
}

void removeLineComment() {
    while (!isEol(tokenStr[tokenIndex])) tokenIndex++;
    if (isEol(tokenStr[tokenIndex])) tokenIndex++;
}

void nextLine() {
    progLineNum++;
    progLineStart = tokenStr + tokenIndex;  //** Pointer op
    isFirstTokenOnLine = true;
}

void removeComment() {
    tokenIndex += 2;        // skip comment start token
    while (!(tokenStr[tokenIndex] == '*' && tokenStr[tokenIndex+1] == '/')) {
        if (isEol(tokenStr[tokenIndex])) {
            nextLine();
        }
        tokenIndex++;
    }
    tokenIndex += 2;        // skip comment end token
}

void tokenizer_nextLine() {
    removeLineComment();
    nextLine();
}

void trimWhitespaceAndComments() {
    /* trim whitespace and kill comments -- in a loop to compensate for multiple lines */

    while (hasToken() && (isWhiteSpace() || isStartOfComment() || isStartOfLineComment())) {
        while (isWhiteSpace()) {
            if (isEol(tokenStr[tokenIndex])) {
                nextLine();
            }
            tokenIndex++;
        }

        // make sure we still have tokens before trying to remove comments
        if (hasToken()) {
            if (isStartOfComment()) removeComment();
            if (isStartOfLineComment()) tokenizer_nextLine();
        }
    }
}

TokenType getDoubleSymbolTokenType(char *token, char firstChar, int tokenEnd) {
    TokenType tokenType = TT_SYMBOL;
    char secondChar = tokenStr[tokenIndex];
    if ((secondChar == '=') && charInCharset(firstChar, "=!<>+-&|")) {
        token[tokenEnd] = secondChar;
        switch(firstChar) {
            case '=': tokenType = TT_EQUAL; break;
            case '!': tokenType = TT_NOT_EQUAL; break;
            case '<': tokenType = TT_LESS_EQUAL; break;
            case '>': tokenType = TT_GREATER_EQUAL; break;
            case '+': tokenType = TT_ADD_TO; break;
            case '-': tokenType = TT_SUB_FROM; break;
            case '&': tokenType = TT_AND_WITH; break;
            case '|': tokenType = TT_OR_WITH; break;
        }
    } else if ((firstChar == secondChar) && charInCharset(firstChar,"><&|+-")) {
        token[tokenEnd] = secondChar;
        switch(firstChar) {
            case '>': tokenType = TT_SHIFT_RIGHT; break;
            case '<': tokenType = TT_SHIFT_LEFT; break;
            case '&': tokenType = TT_BOOL_AND; break;
            case '|': tokenType = TT_BOOL_OR; break;
            case '+': tokenType = TT_INC; break;
            case '-': tokenType = TT_DEC; break;
        }
    }
    return tokenType;
}
