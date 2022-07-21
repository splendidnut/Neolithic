/***************************************************************************
 * Neolithic Compiler - Simple C Cross-compiler for the 6502
 *
 * Copyright (c) 2020-2022 by Philip Blackman
 * -------------------------------------------------------------------------
 *
 * Licensed under the GNU General Public License v2.0
 *
 * See the "LICENSE.TXT" file for more information regarding usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * -------------------------------------------------------------------------
 */

//
//  --- Type List ---
//
//  This is a very simple module designed to keep track of
//  the names of structs and unions to help the parser.
//
// Created by admin on 4/21/2021.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/common.h"
#include "type_list.h"

TypeName* firstTypeName = NULL;
TypeName* lastTypeName = NULL;
int numTypes = 0;

void TypeList_add(char *name) {
#ifdef DEBUG_TYPE_LIST
    printf("TypeList_add: %s\n", name);
#endif

    TypeName *newTypeName = allocMem(sizeof(TypeName));
    newTypeName->name = name;
    newTypeName->next = NULL;

    if (numTypes == 0) {
        firstTypeName = newTypeName;
    } else {
        lastTypeName->next = newTypeName;
    }
    lastTypeName = newTypeName;
    numTypes++;
}

TypeName* TypeList_find(char *name) {
    if (numTypes == 0) return NULL;

    TypeName *curName = firstTypeName;
    while (curName != NULL) {
        if (strcmp(curName->name, name) == 0) return curName;
        curName = curName->next;
    }
    return NULL;
}

void TypeList_cleanUp() {
    TypeName *nextName;
    TypeName *curName = firstTypeName;
    while (curName != NULL) {
        nextName = curName->next;
        free(curName);
        curName = nextName;
    }
}