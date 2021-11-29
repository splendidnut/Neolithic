/*
		Tokenizer interface
*/

#ifndef MODULE_TOKENIZER
#define MODULE_TOKENIZER

#include "tokens.h"
#include "common/common.h"

#define TOKEN_LENGTH_LIMIT 84
#define TOKEN_LINE_LIMIT 120

// TODO: Potentially split builtin tokens from tokenType
typedef enum TokenType {
    //--- very generic token types
    TT_NONE,
    TT_NUMBER,
    TT_SYMBOL,
    TT_STRING,
    TT_IDENTIFIER,
    TT_BUILTIN,

    // -- single symbols
    TT_HASH         = '#',
    TT_AMPERSAND    = '&',
    TT_OPEN_PAREN   = '(',
    TT_CLOSE_PAREN  = ')',
    TT_MULTIPLY     = '*',
    TT_PLUS         = '+',
    TT_COMMA        = ',',
    TT_MINUS        = '-',
    TT_PERIOD       = '.',
    TT_DIVIDE       = '/',
    TT_COLON        = ':',
    TT_SEMICOLON    = ';',
    TT_LESS_THAN    = '<',
    TT_ASSIGN       = '=',
    TT_GREATER_THAN = '>',
    TT_QUESTION     = '?',
    TT_AT_SIGN      = '@',
    TT_OPEN_BRACKET = '[',
    TT_CLOSE_BRACKET= ']',
    TT_OPEN_BRACE   = '{',
    TT_CLOSE_BRACE  = '}',

    // --- reserved words
    TT_ASM = 256,
    TT_ALIAS,
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
    TT_LOOP,
    TT_NEW,
    TT_NULL,
    TT_REGISTER,
    TT_RETURN,
    TT_SIGNED,
    TT_SIZEOF,
    TT_STATIC,
    TT_STROBE,
    TT_STRUCT,
    TT_SWITCH,
    TT_TRUE,
    TT_TYPE,
    TT_TYPEOF,
    TT_UNION,
    TT_UNSIGNED,
    TT_VAR,
    TT_VOID,
    TT_WHILE,
    TT_WORD,
    TT_ZEROPAGE,    // 6502 specific

    // double symbols:      (2 groups)
    //    == != <= >= += -= &= |=   all end in equal char
    //    >> << && || ++ --         both chars are the same

    TT_EQUAL,
    TT_NOT_EQUAL,
    TT_GREATER_EQUAL,
    TT_LESS_EQUAL,
    TT_ADD_TO,
    TT_SUB_FROM,
    TT_AND_WITH,
    TT_OR_WITH,
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
 * TokenObject struct - base object used by the tokenizer to provide the user with a token
 */
typedef struct {
    char tokenStr[TOKEN_LENGTH_LIMIT];
    enum TokenType tokenType;
    TokenFlags tokenFlags;
    int tokenPos;
} TokenObject;

/*-----------------------------------------
	Public Interface
 */

extern void initTokenizer(char *);
extern int getProgLineNum(void);
extern SourceCodeLine getProgramLineString();
extern const char *lookupTokenSymbol(TokenType tokenType);
extern bool hasToken(void);
extern bool inCharset(TokenObject *token, const char *charSet);
extern TokenObject *getToken(void);
extern TokenObject *peekToken(void);
extern char * copyTokenStr(TokenObject *token);
extern int copyTokenInt(TokenObject *token);
extern void killTokenizer(void);
extern TokenType getTokenSymbolType(TokenObject *token);
extern void tokenizer_nextLine();

extern bool isModifierToken(TokenObject *token);
extern bool isTypeToken(TokenObject *token);
extern bool isIdentifier(TokenObject *token);

extern bool isEquality(enum TokenType symType);
extern bool isOpAsgn(enum TokenType symType);
extern bool isShift(enum TokenType symType);
extern bool isBoolAndOr(enum TokenType symType);
extern bool isIncDec(enum TokenType symType);
extern bool isComparison(TokenObject *token);


#endif
