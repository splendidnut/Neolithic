/*
 *   Parser
 *
 *
 *  NOTES to prevent strange errors:
 *
 *    Need to make sure to compile ALL modules with the same memory model
 *           (compact - 64k code, 1Mb data)
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data/syntax_tree.h"
#include "parser/tokenize.h"
#include "data/symbols.h"
#include "parser/parser.h"



//---------------------------------------------------------------------

void showParseTree(ListNode node) {
    if (node.type == N_LIST) {
        List *list = node.value.list;
        
        printf(" as ");
        showList(list);
        printf("\n");
    } else {
        printf("Error: failed to parse\n");
    }
}

char * evaluate(char *str) {
    ListNode node;
    char *token = (char *)malloc(20);
    initTokenizer(str);
    
    strcpy(token, peekToken()->tokenStr);
    
    if (!strcmp(token, "function")) {
        node = parse_stmt_function();
    } else if (isType(token)) {
        // handle var list/initialization
        node = parse_variable();
    } else {
        // handle an expression/assignment statement
        node = parse_expr_assignment();
    }
    
    // now print the parse tree
    showParseTree(node);
    
    killTokenizer();
    
    return str;
}

 
/*---------------------------
//   Main method
*/

void main() {
    char userString[82];
    bool done = false;
    //unsigned long freeMem = coreleft();
    unsigned long freeMem = 0;
    printf("Neolithic Parser -- free memory: %lu\n", freeMem);
    
    printf("Creating symbol table\n");
	initSymbolTable();

	/* read, eval, print, loop */
	while (!done) {
        printf("> ");
        fflush(stdout);
        if (fgets(userString, 80, stdin) != NULL) {
            
            // trim off newline characters
            userString[strcspn(userString, "\n")] = 0; // works for LF, CR, CRLF, LFCR
            
            // check for simple commands, or run evaluator
            if (!strcmp(userString, "exit")) {
                done = true;
            } else if (!strcmp(userString, "mem")) {
                //freeMem = (unsigned long)coreleft();
                printf("free memory: %lu\n", freeMem);
            } else if (!strcmp(userString, "symbols")) {
                showSymbolTable();
            } else {
                printf("%s\n", evaluate(userString));
            }
        } else done = true;
	};
    killSymbolTable();
}