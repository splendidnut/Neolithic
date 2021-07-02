/*
   Parser interface
*/

#ifndef MODULE_PARSER
#define MODULE_PARSER

#include "data/syntax_tree.h"
#include "tokenize.h"

extern int parserErrorCount;

extern bool acceptToken(TokenType tokenType);
extern bool acceptOptionalToken(TokenType tokenType);
extern void printErrorWithSourceLine(const char* errorMsg);
extern void printError(const char* fmtErrorMsg, ...);

extern ListNode parse_codeBlock(void);
extern ListNode parse_stmt_function(char *funcType);
extern ListNode parse_expr_assignment(void);
extern ListNode parse_var_assignment(void);
extern ListNode parse_expr(void);
extern ListNode parse_primary_expr(bool isLValue, bool isExprAllowed, int allowNestedExpr);
extern ListNode parse_variable(void);
extern ListNode parse_stmt_block(void);
extern ListNode parse_program(char *sourceCode);


#endif
