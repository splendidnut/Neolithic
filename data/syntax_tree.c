//
//   Syntax Tree module - supporting functions
//


#include <stdio.h>
#include <stdlib.h>

#include "parser/tokens.h"
#include "syntax_tree.h"
#include "parser/tokenize.h"


//---------------------------------------------
//   Node methods

ListNode createEmptyNode() {
    ListNode result;
    result.value.num = 0;
    result.type  = N_EMPTY;
    return result;
}

ListNode createIntNode(int num) {
    ListNode result;
    result.value.num = num;
    result.type = N_INT;
    return result;
}

ListNode createCharNode(char ch) {
    ListNode result;
    result.value.ch = ch;
    result.type = N_CHAR;
    return result;
}

ListNode createStrNode(const char *str) {
    ListNode result;
    result.value.str = (char *)str;
    result.type = N_STR;
    return result;
}

ListNode createListNode(List *list) {
    ListNode result;
    result.value.list = list;
    result.type = N_LIST;
    return result;
}

ListNode createParseToken(enum ParseToken parseToken) {
    ListNode result;
    result.value.parseToken = parseToken;
    result.type = N_TOKEN;
    return result;
}

/*ListNode createSymbolNode(SymbolRecord *symbolRecord) {
    ListNode result;
    result.value.symbol = symbolRecord;
    result.type = N_SYMBOL;
    return result;
}*/


bool isListNode(ListNode node) {
    return (node.type == N_LIST && node.value.list->nodes[0].type == N_LIST);
}

bool isToken(ListNode node, enum ParseToken parseToken) {
    return (node.type == N_TOKEN && node.value.parseToken == parseToken);
}


//---------------------------------------------
//   List methods

List * createList(int initialSize) {
    List* list = (List *)malloc(sizeof(List) + (initialSize * sizeof(ListNode)));
    list->count = 0;
    list->size = initialSize;
    list->hasNestedList = false;
    list->lineNum = getProgLineNum();
    list->progLine = getProgramLineString();
    return list;
}

int getListCount(List *list) {
    return (list->count);
}

int canAddToList(List *list) {
    return (list->count < list->size);
}

int addNode(List *list, ListNode node) {
    int canAdd = (list != NULL && canAddToList(list));
    if (canAdd) {
        int index = list->count++;
        list->nodes[index].value = node.value;
        list->nodes[index].type = node.type;

        if (node.type == N_LIST) list->hasNestedList = true;
    }
    return canAdd;
}

List *wrapNode(ListNode node) {
    List *wrappedList = createList(1);
    addNode(wrappedList, node);
    return wrappedList;
}


void showList(FILE *outputFile, const List *list, int indentLevel) {
    ListNode node;
    int index;

    // start of new ident level?
    if (indentLevel > 0 && list->hasNestedList) {
        fprintf(outputFile, "\n");
        for (int i=0; i<indentLevel; i++) fprintf(outputFile,"  ");
    }

    fprintf(outputFile, "(");
    if (list->count > 0) for (index=0; index<list->count; index++) {
        if (index>0) {
            fprintf(outputFile, ", ");
        }
        node = list->nodes[index];
        switch (node.type) {
            case N_LIST:
                showList(outputFile, node.value.list, indentLevel+1);
                break;
            case N_STR:
                fprintf(outputFile, "'%s'",node.value.str);
                break;
            case N_CHAR:
                fprintf(outputFile, "'%c'", node.value.ch);
                break;
            case N_EMPTY:
                fprintf(outputFile, "EMPTY");
                break;
            case N_TOKEN:
                fprintf(outputFile, "%s", getParseTokenName(node.value.parseToken));
                break;
            default:
                fprintf(outputFile, "%d",node.value.num);
        }
    }
    fprintf(outputFile, ")");
}

void destroyList(List *list) {
    free(list);
}

