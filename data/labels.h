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
// Created by admin on 4/4/2020.
//

#ifndef MODULE_LABELS_H
#define MODULE_LABELS_H

#include <stdbool.h>

#define LABEL_NAME_LIMIT 64

enum LabelType { LBL_CODE, LBL_LOOP_START, LBL_DATA };

typedef struct LabelStruct {
    char *name;
    enum LabelType type;
    bool hasBeenReferenced;
    bool hasLocation;
    int location;
    struct LabelStruct *link;
    struct LabelStruct *next;
} Label;

extern Label * newGenericLabel(enum LabelType type);
extern Label * newLabel(char* name, enum LabelType type);
extern void linkToLabel(Label *srcLabel, Label *linkedLabel);
extern void addLabelRef(Label *label);
extern Label * findLabel(const char *name);
extern void printLabelList();

#endif //MODULE_LABELS_H
