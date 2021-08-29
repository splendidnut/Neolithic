//
// Created by admin on 6/22/2020.
//

#include <string.h>
#include "data/labels.h"
#include "optimizer.h"
#include "data/instr_list.h"
#include "output/output_block.h"

typedef void (*ProcessInstrFunc)(Instr *);          // pointer to instruction processing function

void WalkInstructions(ProcessInstrFunc instrFunc, InstrBlock *instrBlock) {
    Instr *curInstr = instrBlock->firstInstr;
    while (curInstr != NULL) {
        instrFunc(curInstr);
        curInstr = curInstr->nextInstr;
    }
}

void WalkInstructionsBackwards(ProcessInstrFunc instrFunc, InstrBlock *instrBlock) {
    Instr *curInstr = instrBlock->lastInstr;
    while (curInstr != NULL) {
        instrFunc(curInstr);
        curInstr = curInstr->prevInstr;
    }
}

//-------------------------------------------
const int LABEL_COUNT = 50;
int curLabelIndex;

typedef struct {
    Label *label;
    Instr *instr;
    bool canRemap;      // can be remapped to another label?  (duplicate)
    bool canRemove;     // can be removed?  (unused)

    Label *newLabel;
} LabelInfo;

LabelInfo labelSet[50];

/**
 * Find label destination
 * @param labelName
 * @return instruction at label destination
 */

Instr *findLabelDestination(const char *labelName) {
    for (int labelIndex = 0; (labelSet[labelIndex].label != NULL) && (labelIndex < LABEL_COUNT); labelIndex++) {
        LabelInfo labelInfo = labelSet[labelIndex];
        if (strcmp(labelInfo.label->name, labelName) == 0) {
            return labelInfo.instr;
        }
    }
    return NULL;
}

LabelInfo *findLabelInfo(const char *labelName) {
    for (int labelIndex = 0; (labelSet[labelIndex].label != NULL) && (labelIndex < LABEL_COUNT); labelIndex++) {
        if (strcmp(labelSet[labelIndex].label->name, labelName) == 0) {
            return &labelSet[labelIndex];
        }
    }
    return NULL;
}


/**
 * Find All Labels and generate re-mappings to remove redundant ones
 *
 * @param curInstr
 */

void FindAllLabels(Instr *curInstr) {
    static bool lastInstrNone = false;
    static Label *lastLabel = NULL;
    if (curInstr->label != NULL) {
        labelSet[curLabelIndex].label = curInstr->label;
        labelSet[curLabelIndex].instr = curInstr;
        labelSet[curLabelIndex].canRemap = lastInstrNone;
        if (lastInstrNone) {
            labelSet[curLabelIndex].newLabel = lastLabel;
        }
        curLabelIndex++;

        if (!lastInstrNone) lastLabel = curInstr->label;
        lastInstrNone = (curInstr->mne == MNE_NONE);
    }
}

void clearLabelInfo(int index) {
    labelSet[index].label = NULL;
    labelSet[index].instr = NULL;
    labelSet[index].newLabel = NULL;
}

void PrintLabelUsage(Instr *curInstr) {
    bool isJump = (curInstr->mne == JMP) || (curInstr->mne == JSR);
    if (isBranch(curInstr->mne) || isJump) {
        printf("   %s\n", curInstr->paramName);
    }
}

/**
 * Replace label references in an instuction block
 * @param instrBlock
 * @param oldLabel
 * @param newLabel
 */

void RemapLabel(InstrBlock *instrBlock, Label *oldLabel, Label *newLabel) {
    Instr *curInstr = instrBlock->firstInstr;
    while (curInstr != NULL) {
        bool isJump = (curInstr->mne == JMP) || (curInstr->mne == JSR);
        if (isBranch(curInstr->mne) || isJump) {
            if ((curInstr->paramName != NULL)
                && (strncmp(curInstr->paramName, oldLabel->name, 30) == 0)) {
                curInstr->paramName = newLabel->name;
            }
        }
        if (curInstr->label != NULL && (strcmp(curInstr->label->name, oldLabel->name) == 0)) {
            curInstr->label = NULL;
        }
        curInstr = curInstr->nextInstr;
    }
}

void print_labelList() {
    printf("Label List\n------------\n");
    for (int labelIndex = 0; (labelSet[labelIndex].label != NULL) && (labelIndex < LABEL_COUNT); labelIndex++) {
        LabelInfo labelInfo = labelSet[labelIndex];
        Label *curLabel = labelSet[labelIndex].label;
        bool hasNewLabel = (labelInfo.newLabel != NULL && labelInfo.canRemap);
        printf("   %-20s:  %4s  %5s  %s\n",
               curLabel->name,
               getMnemonicStr(labelInfo.instr->mne),
               (labelInfo.canRemap ? "true" : "false"),
               hasNewLabel ? labelInfo.newLabel->name : "");
    }
    printf("\n");
}

void remap_labels(InstrBlock *curBlock) {
    for (int labelIndex = 0; (labelSet[labelIndex].label != NULL) && (labelIndex < LABEL_COUNT); labelIndex++) {
        LabelInfo labelInfo = labelSet[labelIndex];
        Label *curLabel = labelSet[labelIndex].label;
        bool hasNewLabel = (labelInfo.newLabel != NULL && labelInfo.canRemap);
        if (hasNewLabel) {
            RemapLabel(curBlock, curLabel, labelInfo.newLabel);
        }
    }
}

void mark_all_labels_unused() {
    for (int labelIndex = 0; (labelSet[labelIndex].label != NULL) && (labelIndex < LABEL_COUNT); labelIndex++) {
        labelSet[labelIndex].canRemove = true;
    }
}

/**
 * Optimize label usage:
 *    - Remove labels sharing the same spot
 */

void OPT_FindAllLabels(InstrBlock *instrBlock) {
    // collect all labels
    curLabelIndex = 0;
    clearLabelInfo(curLabelIndex);
    WalkInstructions(&FindAllLabels, instrBlock);
    clearLabelInfo(curLabelIndex);
}

void OPT_RemapLabelsInBlock(InstrBlock *instrBlock) {
    /*/ print out label list
    print_labelList(); /*/

    // remap labels
    remap_labels(instrBlock);

    /*/ print out all label usages
    printf("Label usage\n--------------\n");
    WalkInstructions(&PrintLabelUsage, instrBlock);
     */
}


void MarkLabelsUsed(Instr *curInstr) {
    bool isJump = (curInstr->mne == JMP) || (curInstr->mne == JSR);
    if ((isBranch(curInstr->mne) || isJump) && (curInstr->paramName != NULL)) {
        // mark label as used
        LabelInfo *labelInfo = findLabelInfo(curInstr->paramName);
        if (labelInfo != NULL) {
            labelInfo->canRemove = false;
        }
    }
}

void RemoveUnusedLabels(Instr *curInstr) {
    if (curInstr->label != NULL) {
        LabelInfo *labelInfo = findLabelInfo(curInstr->label->name);
        if (labelInfo->canRemove) {
            curInstr->label = NULL;
        }
    }
}

void OPT_RemoveUnusedLabels(InstrBlock *instrBlock) {
    // mark all labels unused
    mark_all_labels_unused();

    // first label cannot be removed (name of function)
    labelSet[0].canRemove = false;

    // walk instructions to find used labels
    WalkInstructions(&MarkLabelsUsed, instrBlock);

    // walk instructions to remove unused labels
    WalkInstructions(&RemoveUnusedLabels, instrBlock);
}


/**
 * Optimize jumps to RTS instructions
 */

void ReplaceJumpsToRTS(Instr *curInstr) {
    bool isJump = (curInstr->mne == JMP) || (curInstr->mne == JSR);
    if (isJump && (curInstr->paramName != NULL)) {
        Instr *destInstr = findLabelDestination(curInstr->paramName);
        if ((destInstr != NULL) && (destInstr->mne == RTS)) {
            // replace JMP with RTS
            curInstr->mne = RTS;
            curInstr->addrMode = ADDR_NONE;
        }
    }
}

void ReplaceJSRwithJMP(Instr *curInstr) {
    if ((curInstr->mne == JSR) && (curInstr->nextInstr != NULL) && (curInstr->nextInstr->mne == RTS)) {
        curInstr->mne = JMP;
        if (curInstr->nextInstr->label == NULL) {
            curInstr->nextInstr->mne = MNE_NONE;
        }
    }
}

void ReplaceDoubleJMP(Instr *curInstr) {
    if (curInstr->mne == JMP && (curInstr->paramName != NULL)) {
        Instr *destInstr = findLabelDestination(curInstr->paramName);
        if ((destInstr != NULL) && (destInstr->mne == JMP)) {
            // replace double JMP with single JMP
            curInstr->paramName = destInstr->paramName;
        }
    }
}

void OPT_Jumps(InstrBlock *instrBlock) {
    WalkInstructionsBackwards(&ReplaceJumpsToRTS, instrBlock);
    WalkInstructions(&ReplaceJSRwithJMP, instrBlock);
    WalkInstructions(&ReplaceDoubleJMP, instrBlock);
}

/**
 *  Remove Empty instructions (MNE_NONE)
 *
 */

void RemoveEmptyInstrs(Instr *curInstr) {
    if (curInstr->mne != MNE_NONE) return;

    Instr *nextInstr = curInstr->nextInstr;

    if (curInstr->label == NULL) {
        // remove current instruction
        curInstr->prevInstr->nextInstr = nextInstr;
    } else if ((curInstr->label != NULL)
                && (nextInstr->label == NULL)) {
                //&& (curInstr->prevInstr != NULL)) {
        nextInstr->label = curInstr->label;
        curInstr->prevInstr->nextInstr = nextInstr;
    }
}

void OPT_RemoveEmptyInstrs(InstrBlock *instrBlock) {
    WalkInstructionsBackwards(&RemoveEmptyInstrs, instrBlock);
}


/**
 *  Fit program code blocks into pages to avoid branch issues
 *
 */
void OPT_FitInPages() {
    const OutputBlock *block = OB_getFirstBlock();
    while (block != NULL) {

        block = block->nextBlock;
    }
}

void OptimizeDecCmpZero(Instr *curInstr) {
    Instr *instr2 = curInstr->nextInstr;
    if (instr2 == NULL) return;
    Instr *instr3 = instr2->nextInstr;
    if (instr3 == NULL) return;

    // check for sequence
    //    DEC paramName
    //    LDA paramName
    //    BNE/BEQ loc

    if ((curInstr->mne == DEC) && (curInstr->paramName != NULL)
        && (instr2->mne == LDA) && (instr2->paramName != NULL)
            && (strcmp(curInstr->paramName, instr2->paramName)==0)
        && ((instr3->mne == BNE) || (instr3->mne == BEQ))) {
        // we can skip the LDA since DEC has set the flags already
        curInstr->nextInstr = instr3;
    }
}


/**
 * MAIN OPTIMIZER
 */

void OPT_CodeBlock(OutputBlock *curBlock) {
    InstrBlock *instrBlock = curBlock->codeBlock;

    OPT_FindAllLabels(instrBlock);
    OPT_Jumps(instrBlock);
    OPT_RemapLabelsInBlock(instrBlock);

    OPT_RemoveEmptyInstrs(instrBlock);
    OPT_RemoveUnusedLabels(instrBlock);
}

void OPT_DecCmp(OutputBlock *curBlock) {
    InstrBlock *instrBlock = curBlock->codeBlock;

    WalkInstructions(&OptimizeDecCmpZero, instrBlock);
}

void OPT_RunOptimizer() {
    // optimize labels - currently done twice to catch what's missed on the first go-round
    OB_WalkCodeBlocks(&OPT_CodeBlock);
    OB_WalkCodeBlocks(&OPT_CodeBlock);

    //----------
    OB_WalkCodeBlocks(&OPT_DecCmp);
}

