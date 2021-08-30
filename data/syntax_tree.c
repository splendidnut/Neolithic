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

ListNode createMnemonicNode(enum MnemonicCode mne) {
    ListNode result;
    result.value.mne = mne;
    result.type = N_MNE;
    return result;
}

ListNode createAddrModeNode(enum AddrModes addrMode) {
    ListNode result;
    result.value.addrMode = addrMode;
    result.type = N_ADDR_MODE;
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
//  List/Tree Memory allocator

static unsigned int treeMemoryUsed = 0;
static unsigned int treeMaxMemoryUsed = 0;
static unsigned int treeListCount = 0;
static unsigned int treeLargestChunk = 0;

void *TREE_allocMem(unsigned int size) {
    if (size > treeLargestChunk) treeLargestChunk = size;
    treeMemoryUsed += size;
    if (treeMemoryUsed > treeMaxMemoryUsed) treeMaxMemoryUsed = treeMemoryUsed;
    treeListCount++;
    return malloc(size);
}

void TREE_freeMem(List *mem) {
    treeMemoryUsed -= (sizeof (List) + (mem->size * sizeof(ListNode)));
    free(mem);
}

void printParseTreeMemUsage() {
    printf("\nParse Tree objects: %d", treeListCount);
    printf("\nParse Tree largest object: %d bytes", treeLargestChunk);
    printf("\nParse Tree memory usage: %d  (max: %d)\n", treeMemoryUsed, treeMaxMemoryUsed);
}

//---------------------------------------------
//   List methods

List * createList(int initialSize) {
    List* list = (List *)TREE_allocMem(sizeof(List) + (initialSize * sizeof(ListNode)));
    list->count = 0;
    list->size = initialSize;
    list->hasNestedList = false;
    list->lineNum = getProgLineNum();
    list->progLine = getProgramLineString();
    return list;
}

/**
 * Return a condensed version of the provided list
 *
 * Takes provided list, copies everything to a new list that's just big enough
 * to fit everything, and then frees the old list.
 *
 * @param inList
 * @return condensed version of inList
 */
List * condenseList(List *inList) {
    List *outList = createList(inList->count);
    outList->count = inList->count;
    outList->size = inList->size;
    outList->hasNestedList = inList->hasNestedList;
    outList->lineNum = inList->lineNum;
    outList->progLine = inList->progLine;
    for_range (cnt, 0, inList->count) {
        outList->nodes[cnt] = inList->nodes[cnt];
    }
    TREE_freeMem(inList);
    return outList;
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

void unwarpNodeList(List *nodeList, ListNode *node) {
    if (node->type != N_EMPTY) {
        // process compound stmt - mainly for multiple vars declared on a single line (comma separated list)
        if (isListNode((*node))) {
            List *subNodeList = node->value.list;
            for (int cnt=0; cnt<subNodeList->count; cnt++) {
                addNode(nodeList, subNodeList->nodes[cnt]);
            }
        } else {
            // add normal node to program (for most cases)
            addNode(nodeList, *node);
        }
    }
}


void reverseList(List *list) {
    int startPoint = 1;
    int endPoint = list->count-1;
    while (startPoint < endPoint) {
        ListNode listNode = list->nodes[startPoint];
        list->nodes[startPoint] = list->nodes[endPoint];
        list->nodes[endPoint] = listNode;

        startPoint++;
        endPoint--;
    }
}

void showNode(FILE *outputFile, ListNode node, int indentLevel) {
    switch (node.type) {
        case N_LIST:
            showList(outputFile, node.value.list, indentLevel + 1);
            break;
        case N_STR:
            fprintf(outputFile, "'%s'", node.value.str);
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
        case N_MNE:
            fprintf(outputFile, "%s", getMnemonicStr(node.value.mne));
            break;
        case N_ADDR_MODE:
            fprintf(outputFile, "%s", getAddrModeSt(node.value.addrMode).name);
            break;
        default:
            fprintf(outputFile, "%d", node.value.num);
    }
}

void showList(FILE *outputFile, const List *list, int indentLevel) {
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
        showNode(outputFile, list->nodes[index], indentLevel);
    }
    fprintf(outputFile, ")");
}

void destroyList(List *list) {
    free(list);
}

