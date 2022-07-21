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
//  Keep track of user defined types (structs and unions)
//
// Created by admin on 4/21/2021.
//

#ifndef TYPE_LIST_H
#define TYPE_LIST_H

typedef struct TypeNameStruct {
    char *name;
    struct TypeNameStruct *next;
} TypeName;

extern void TypeList_add(char *name);
extern TypeName* TypeList_find(char *name);
extern void TypeList_cleanUp();

#endif //TYPE_LIST_H
