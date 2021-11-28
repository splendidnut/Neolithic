//
// .Neolithic Compiler Module - Flatten Tree
//
//  This module is used for flattening an expression tree to allow
//  easier implementation of 16-bit code generation.
//
// Created by pblackman on 8/3/21.
//


#include "flatten_tree.h"
#include <data/syntax_tree.h>

//#define DEBUG_EXPR_FLATTEN

//------------------------------------
//  Module-local variables

static FILE *debugOutput;
static int flattenedExprCount;

//  x = y*2 + 2*3;
//  [set, x, [+, [*, y, 2], [*, 2, 3]]]
//
//  [set, acc, y]
//  [*, acc, 2]
//
//  == [op, expr, expr] - two branches, need temp
//  [define, temp]
//  [set, temp, acc]
//
//  [set, acc, 2]
//  [*, acc, 3]
//
//  ==[op, expr, expr]
//  ==[+, temp, acc]
//  [set, acc, [+, temp, acc]]
//  [mark_free, temp]
//
//  [set, x, acc]

//-------------------------------
//  x = (y+1) * 2
// [set, x, [*, [+, y, 1], 2]]
//
// [load, acc, y]
// [add, acc, 1]
// [mul, acc, 2]
// [set, x, acc]
//
//--------------------------------
//  x = y * (2 + z)
// [set, x, [*, y, [+, 2, z]]]
//
// [load, acc, y]
// [define, temp, acc]
// [load, acc, 2]
// [add, acc, z]
// [mul, temp, acc]
// [set, x, acc]

List* exprList;
int numTemps;

void FT_MoveAccToTemp(ListNode tempNode) {
    List *copyExpr = createList(3);
    addNode(copyExpr, createParseToken(PT_SET));
    addNode(copyExpr, tempNode);
    addNode(copyExpr, createStrNode("acc"));

    showList(debugOutput, copyExpr, 2);
    fprintf(debugOutput," -- copyExpr\n");
    addNode(exprList, createListNode(copyExpr));
}

List* FT_Expression(const List *expr) {
    ListNode leftNode = expr->nodes[1];
    ListNode rightNode = expr->nodes[2];

    bool isLeftExpr = (leftNode.type == N_LIST) && (!isToken(leftNode.value.list->nodes[0], PT_PROPERTY_REF));
    bool isRightExpr = (rightNode.type == N_LIST) && (!isToken(rightNode.value.list->nodes[0], PT_PROPERTY_REF));

    // walk left,
    if (isLeftExpr) {
        FT_Expression(leftNode.value.list);
        leftNode = createStrNode("acc");
    }

    // walk right,
    if (isRightExpr) {

        // if left node was an expression, we need to copy A reg into a temp variable
        if (isLeftExpr) {
            ListNode tempNode = createStrNode("temp");
            FT_MoveAccToTemp(tempNode);
            leftNode = tempNode;
        }

        FT_Expression(rightNode.value.list);
        rightNode = createStrNode("acc");
    }

    List *nodeExpr = createList(3);
    addNode(nodeExpr, expr->nodes[0]);
    addNode(nodeExpr, leftNode);
    addNode(nodeExpr, rightNode);

    showList(debugOutput, nodeExpr, 2);
    fprintf(debugOutput," -- nodeExpr\n");

    addNode(exprList, createListNode(nodeExpr));
    return nodeExpr;
}

/**
 * Flatten an expression
 *
 * @param expr - expression to flatten
 * @param origStmt - original statement containing expression (only used for displaying source code line)
 * @return
 */
List* flatten_expression(const List *expr, const List *origStmt) {

    // filter out any expressions that don't need to be flattened
    ListNode leftNode = expr->nodes[1];
    ListNode rightNode = expr->nodes[2];

    // if both nodes aren't sub-expressions, we need to check to see
    //   if expression is complex enough to require flattening
    if (!((leftNode.type == N_LIST) && (rightNode.type == N_LIST))) {

        if ((leftNode.type != N_LIST) && (rightNode.type != N_LIST)) return NULL;

        // check if complex node is just a property reference...
        if ((leftNode.type == N_LIST) && (isToken(leftNode.value.list->nodes[0], PT_PROPERTY_REF))) return NULL;
        if ((rightNode.type == N_LIST) && (isToken(rightNode.value.list->nodes[0], PT_PROPERTY_REF))) return NULL;
    }

    // now we can attempt to flatten the expression
    flattenedExprCount++;

    numTemps = 0;
    exprList = createList(20);  // TODO: make list auto expand

    if (debugOutput == NULL) debugOutput = stdout;

#ifdef DEBUG_EXPR_FLATTEN
    fprintf(debugOutput,"-----------------------------------------------\nStarting with:\n");
    fprintf(debugOutput,"%s\n", buildSourceCodeLine(&origStmt->progLine));
    showList(debugOutput, expr, 2);
    fprintf(debugOutput,"\n\nProduces intermediates:\n");
    FT_Expression(expr);
    fprintf(debugOutput,"\n\nProduces:\n");
    showList(debugOutput, exprList, 2);
    fprintf(debugOutput,"\n\n");
#else
    FT_Expression(expr);
#endif
    return exprList;
}

void FE_initDebugger() {
#ifdef DEBUG_EXPR_FLATTEN
    debugOutput = fopen("debug_flat_expr.txt", "wb");
#endif
    flattenedExprCount = 0;
}

void FE_killDebugger() {
#ifdef DEBUG_EXPR_FLATTEN
    fprintf(debugOutput, "\nExpressions Flattened: %d\n", flattenedExprCount);
    if (debugOutput != stdout) fclose(debugOutput);
#endif
}