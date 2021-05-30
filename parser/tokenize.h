/*
		Tokenizer interface
*/

#ifndef MODULE_TOKENIZER
#define MODULE_TOKENIZER

#include "tokens.h"
#include "common/common.h"

#define TOKEN_LENGTH_LIMIT 80
#define TOKEN_LINE_LIMIT 120

typedef enum TokenType {
    //--- very generic token types
    TT_NONE,
    TT_NUMBER,
    TT_SYMBOL,
    TT_STRING,
    TT_IDENTIFIER,

    // --- reserved words
    TT_ASM,
    TT_BOOLEAN,
    TT_BREAK,
    TT_BYTE,
    TT_CASE,
    TT_CHAR,
    TT_CONST,
    TT_DEFAULT,
    TT_DELETE,
    TT_DO,
    TT_ELSE,
    TT_ENUM,
    TT_EXTERN,
    TT_FALSE,
    TT_FOR,
    TT_FUNCTION,
    TT_IF,
    TT_IMPORT,
    TT_IN,
    TT_INLINE,
    TT_INT,
    TT_NEW,
    TT_NULL,
    TT_REGISTER,
    TT_RETURN,
    TT_SIGNED,
    TT_STATIC,
    TT_STROBE,
    TT_STRUCT,
    TT_SWITCH,
    TT_TRUE,
    TT_TYPE,
    TT_UNION,
    TT_UNSIGNED,
    TT_VAR,
    TT_VOID,
    TT_WHILE,
    TT_WORD,
    TT_ZEROPAGE,    // 6502 specific

    // -- single symbols
    TT_LESS_THAN,
    TT_GREATER_THAN,
    TT_PLUS,
    TT_MINUS,

    // double symbols:      (2 groups)
    //    == != <= >= += -=   all end in equal char
    //    >> << && || ++ --   both chars are the same

    TT_EQUAL,
    TT_NOT_EQUAL,
    TT_GREATER_EQUAL,
    TT_LESS_EQUAL,
    TT_ADD_TO,
    TT_SUB_FROM,
    TT_SHIFT_RIGHT,
    TT_SHIFT_LEFT,
    TT_BOOL_AND,
    TT_BOOL_OR,
    TT_INC,
    TT_DEC,

    //---------------------------------
    //  Tokens created by the parser

} TokenType;


typedef enum { TF_NONE, TF_OP, TF_MODIFIER, TF_TYPE } TokenFlags;


/**
 * TokenSymbol struct -
 *
 * object from a static data table providing more specific information
 * about the token that was captured
 *
 */
typedef struct TokenSymbolStruct {
    char *symbol;
    enum TokenType tokenType;
    TokenFlags tokenFlags;
} TokenSymbol;

/**
 * TokenObject struct - base object used by the tokenizer to provide the user with a token
 */
typedef struct TokenObjectStruct {
    int tokenPos;
    enum TokenType tokenType;
    TokenSymbol *tokenSymbol;
    //enum ParseToken token;
	char tokenStr[TOKEN_LENGTH_LIMIT];
} TokenObject;

/*-----------------------------------------
	Public Interface
 */

extern void initTokenizer(char *);
extern int getProgLineNum(void);
extern char* getCurrentProgLine(void);
extern SourceCodeLine getProgramLineString();
extern TokenSymbol * findTokenSymbol(char *tokenName);
extern const char *lookupTokenSymbol(TokenObject *token);
extern int hasToken(void);
extern bool inCharset(TokenObject *token, const char *charSet);
extern TokenObject *getToken(void);
extern TokenObject *peekToken(void);
extern char * copyTokenStr(TokenObject *token);
extern int copyTokenInt(TokenObject *token);
extern void killTokenizer(void);

extern bool isModifierToken(TokenObject *token);
extern bool isTypeToken(TokenObject *token);
extern bool isIdentifier(TokenObject *token);
extern bool isEquality(enum TokenType symType);
extern bool isAddSubAsgn(enum TokenType symType);
extern bool isShift(enum TokenType symType);
extern bool isBoolAndOr(enum TokenType symType);
extern bool isIncDec(enum TokenType symType);
extern bool isComparison(TokenObject *token);


#endif
