//
// Created by admin on 4/4/2020.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    char *generatedName = malloc(12);
    sprintf(generatedName, "L%04X", labelCount);
    return generatedName;
}

Label * newGenericLabel(enum LabelType type) {
    Label *label = malloc(sizeof(struct LabelStruct));
    label->name = genLabelName();
    label->link = NULL;
    label->hasBeenReferenced = false;
    addToLabelList(label);
    return label;
}

Label * newLabel(char* name, enum LabelType type) {
    Label *label = malloc(sizeof(struct LabelStruct));
    label->name = strdup(name);
    label->type = type;
    label->link = NULL;
    label->hasBeenReferenced = false;
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


Label * findLabel(char *name) {
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