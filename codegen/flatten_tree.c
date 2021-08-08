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

List* FT_Expression(const List *expr) {
    ListNode leftNode = expr->nodes[1];
    ListNode rightNode = expr->nodes[2];

    bool isLeftExpr = (leftNode.type == N_LIST) && (!isToken(leftNode.value.list->nodes[0], PT_PROPERTY_REF));
    bool isRightExpr = (rightNode.type == N_LIST) && (!isToken(rightNode.value.list->nodes[0], PT_PROPERTY_REF));

    // walk left,
    if (isLeftExpr) {
        leftNode = createListNode(FT_Expression(leftNode.value.list));
        addNode(exprList, leftNode);
        leftNode = createStrNode("acc");
    }

    // walk right,
    if (isRightExpr) {
        if (isLeftExpr) {
            List *copyExpr = createList(3);
            addNode(copyExpr, createParseToken(PT_SET));
            addNode(copyExpr, createStrNode("temp"));
            addNode(copyExpr, createStrNode("acc"));

            showList(stdout, copyExpr, 2);
            printf("\n");

            addNode(exprList, createListNode(copyExpr));
            leftNode = createStrNode("temp");
        }
        rightNode = createListNode(FT_Expression(rightNode.value.list));
        addNode(exprList, rightNode);
        rightNode = createStrNode("acc");
    }

    List *nodeExpr = createList(3);
    addNode(nodeExpr, expr->nodes[0]);
    addNode(nodeExpr, leftNode);
    addNode(nodeExpr, rightNode);

    showList(stdout, nodeExpr, 2);
    printf("\n");

    addNode(exprList, createListNode(nodeExpr));
    return nodeExpr;
}

List* flatten_expression(const List *expr) {
    numTemps = 0;
    exprList = createList(20);  // TODO: make list auto expand

    printf("Starting with:\n");
    showList(stdout, expr, 2);
    printf("\n\nProduces intermediates:\n");
    FT_Expression(expr);
    printf("\n\nProduces:\n");
    showList(stdout, exprList, 2);
    printf("\n");
    return exprList;
}