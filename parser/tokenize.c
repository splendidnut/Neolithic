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


/*------------------------------------------
     Local Variables
*/
static TokenObject *currentToken;
static char *tokenStr;
static int tokenStrLen;
static int tokenIndex;
static bool isFirstTokenOnLine;

// keep track of program line info for error messages (currently ONLY for the tokenizer/parser)
static char *curProgLine;       // contents of current program line
static char *progLineStart;     // start in file buffer for current program line
static int progLineNum;         // line number of current program line

static const TokenSymbol TokenSymbols[] = {
        {"", TT_NONE, TF_NONE},

        // single symbols
        {"<", TT_LESS_THAN,         TF_OP},
        {">", TT_GREATER_THAN,      TF_OP},
        {"+", TT_PLUS,              TF_OP},
        {"-", TT_MINUS,             TF_OP},

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
        {"static",  TT_STATIC,      TF_MODIFIER},
        {"strobe",  TT_STROBE,      TF_OP},
        {"struct",  TT_STRUCT,      TF_MODIFIER},
        {"switch",  TT_SWITCH,      TF_OP},
        {"true",    TT_TRUE,        TF_OP},
        {"type",    TT_TYPE,        TF_OP},
        {"union",   TT_UNION,       TF_OP},
        {"unsigned",TT_UNSIGNED,    TF_MODIFIER},
        {"var",     TT_VAR,         TF_OP},
        {"void",    TT_VOID,        TF_TYPE},
        {"while",   TT_WHILE,       TF_OP},
        {"word",    TT_WORD,        TF_TYPE},
        {"zeropage",TT_ZEROPAGE,    TF_MODIFIER},
};

static const int NumTokenSymbols = sizeof(TokenSymbols) / sizeof(TokenSymbol);

//TokenObject EMPTY_TOKEN = {-1, TT_NONE, 0, ""};

/* ------------------------------------------
 *    Internal Function declarations
 *
 */

void trimWhitespaceAndComments();
TokenType getDoubleSymbolTokenType(char *token, char firstChar, int tokenEnd);

/* ------------------------------------------
        Functions
 */

TokenSymbol * findTokenSymbol(char *tokenName) {
    int index;
    for (index=1; index < NumTokenSymbols; index++) {
        if (strncmp(tokenName, TokenSymbols[index].symbol, TOKEN_LENGTH_LIMIT) == 0) break;
    }
    if (index >= NumTokenSymbols) index = 0;
    return (TokenSymbol *)&TokenSymbols[index];
}

const char *lookupTokenSymbol(TokenObject *token) {
    int i=0;
    do {
        if (TokenSymbols[i].tokenType == token->tokenType)
            return TokenSymbols[i].symbol;
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
	currentToken = (TokenObject *)malloc(TOKEN_LINE_LIMIT);

	// alloc memory for currentLine
    curProgLine = (char *)malloc(TOKEN_LINE_LIMIT + 10);        // Need space for Line Number

    isFirstTokenOnLine = true;
}

void killTokenizer() {
    /* dealloc currentToken */
    free(curProgLine);
    free(currentToken);
}

int getProgLineNum() {
    return progLineNum;
}

char* getCurrentProgLine() {
    int col = 0;
    while (progLineStart[col] != '\n' && col < (TOKEN_LINE_LIMIT-2)) col++;

    strncpy(curProgLine, progLineStart, col);
    curProgLine[col] = '\n';
    curProgLine[col+1] = '\0';

    return curProgLine;
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

        strncpy(curProgLine, progLineStart, col);
        curProgLine[col] = '\n';
        curProgLine[col + 1] = '\0';

    } else {
        curProgLine[0] = '\0';
    }

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

int hasToken() {
    return (tokenIndex < tokenStrLen-1) && (tokenStr[tokenIndex] != '\0');
}

int charInCharset(char ch, const char *charSet) {
    int result = 0;
    while ((*charSet != '\0') && !result) {
        if (ch == *charSet) result = 1;
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
    currentToken->tokenType = TT_NONE;
    currentToken->tokenStr[0] = '\0';
    currentToken->tokenPos = tokenIndex;
    currentToken->tokenSymbol = (TokenSymbol *)&TokenSymbols[0];
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
                    }
                }
            }
            break;
	}
    token[tokenEnd] = '\0';

    if (tokenEnd > 0) {
        currentToken->tokenSymbol = findTokenSymbol(token);
        isFirstTokenOnLine = false;
    }
    return currentToken;
}

TokenObject *getToken() {
    trimWhitespaceAndComments();
    TokenObject *token = parseToken();
    return token;
}

TokenObject *peekToken() {
    trimWhitespaceAndComments();

    int savedIndex = tokenIndex;
    TokenObject *token = parseToken();
    tokenIndex = savedIndex;
    return token;
}

char * copyTokenStr(TokenObject *token) {
    assert(token != NULL);
    char *copyOfTokenStr = (char *)malloc(TOKEN_LENGTH_LIMIT);
    strncpy(copyOfTokenStr, token->tokenStr, TOKEN_LENGTH_LIMIT - 2);
    copyOfTokenStr[TOKEN_LENGTH_LIMIT-1] = '\0';
    return copyOfTokenStr;
}

int copyTokenInt(TokenObject *token) {
    assert(token != NULL);
    int resultInt = 0;
    char str[11];
    strncpy(str, token->tokenStr, 10);
    if (str[1] == 'x' && str[0] == '0') {
        resultInt = strtol(str + 2, NULL, 16);    // hexadecimal
    } else if (str[1] == 'b' && str[0] == '0') {
        resultInt = strtol(str + 2, NULL, 2);    // binary
    } else if (str[0] == '$') {
        resultInt = strtol(str + 1, NULL, 16);    // ASM style hex
    } else if (str[0] == '%') {
        resultInt = strtol(str + 1, NULL, 2);   // ASM style binary
    } else {
        resultInt = strtol(str, NULL, 10);
    }
    return resultInt;
}


//---------------------------------------------------------
//  Simple token test/compare functions

bool isModifierToken(TokenObject *token) {
    return token->tokenSymbol && (token->tokenSymbol->tokenFlags == TF_MODIFIER);
}

bool isTypeToken(TokenObject *token) {
    return token->tokenSymbol && (token->tokenSymbol->tokenFlags == TF_TYPE);
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
bool isAddSubAsgn(enum TokenType symType) {
    return ((symType == TT_ADD_TO) || (symType == TT_SUB_FROM));
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
    return (tokenStr[tokenIndex] == '/' && tokenStr[tokenIndex+1] == '/') ||
           (tokenStr[tokenIndex] == '#' && isFirstTokenOnLine);
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
            if (isStartOfLineComment()) {
                removeLineComment();
                nextLine();
            }
        }
    }
}

TokenType getDoubleSymbolTokenType(char *token, char firstChar, int tokenEnd) {
    TokenType tokenType = TT_SYMBOL;
    char secondChar = tokenStr[tokenIndex];
    if ((secondChar == '=') && charInCharset(firstChar, "=!<>+-")) {
        token[tokenEnd] = secondChar;
        switch(firstChar) {
            case '=': tokenType = TT_EQUAL; break;
            case '!': tokenType = TT_NOT_EQUAL; break;
            case '<': tokenType = TT_LESS_EQUAL; break;
            case '>': tokenType = TT_GREATER_EQUAL; break;
            case '+': tokenType = TT_ADD_TO; break;
            case '-': tokenType = TT_SUB_FROM; break;
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
