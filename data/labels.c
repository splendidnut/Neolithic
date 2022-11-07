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

#include <stdio.h>
#include <string.h>

#include "common/common.h"
#include "labels.h"

static int labelCount = 0;

static Label *firstLabel;
static Label *lastLabel;

void addToLabelList(Label *label) {
    if (firstLabel && lastLabel) {
        lastLabel->next = label;
        lastLabel = label;
    } else {
        firstLabel = label;
        lastLabel = label;
    }
    lastLabel->next = NULL;
    labelCount++;
}

char *genLabelName() {
    char *generatedName = allocMem(6);
    sprintf(generatedName, "L%04X", labelCount);
    return generatedName;
}

Label * newGenericLabel(enum LabelType type) {
    Label *label = allocMem(sizeof(struct LabelStruct));
    label->name = genLabelName();
    label->type = type;
    label->link = NULL;
    label->hasBeenReferenced = false;
    label->hasLocation = false;
    label->location = 0;
    addToLabelList(label);
    return label;
}

Label * newLabel(char* name, enum LabelType type) {
    Label *label = allocMem(sizeof(struct LabelStruct));
    label->name = name;
    label->type = type;
    label->link = NULL;
    label->hasBeenReferenced = false;
    label->hasLocation = false;
    label->location = 0;
    addToLabelList(label);
    return label;
}

/**
 * Link srcLabel to linkedLabel
 * @param srcLabel label that needs to be linked
 * @param linkedLabel label to link to
 */
void linkToLabel(Label *srcLabel, Label *linkedLabel) {
    srcLabel->link = linkedLabel;
}

/**
 * When a label has been referenced, call this
 * @param label
 */
void addLabelRef(Label *label) {
    label->hasBeenReferenced = true;
}


Label * findLabel(const char *name) {
    Label *curLabel = firstLabel;
    while (curLabel && strncmp(curLabel->name, name, LABEL_NAME_LIMIT) != 0) {
        curLabel = curLabel->next;
    }
    return curLabel;
}

void printLabelList() {
    printf("Labels:\n");
    Label *curLabel = firstLabel;
    while (curLabel) {
        printf("  - %s", curLabel->name);
        curLabel = curLabel->next;
    }
}