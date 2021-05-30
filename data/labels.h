//
// Created by admin on 4/4/2020.
//

#ifndef MODULE_LABELS_H
#define MODULE_LABELS_H

#include <stdbool.h>

#define LABEL_NAME_LIMIT 32

enum LabelType { L_CODE, L_DATA };

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
extern Label * findLabel(char *name);
extern void printLabelList();

#endif //MODULE_LABELS_H
