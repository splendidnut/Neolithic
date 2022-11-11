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
//  Code Optimizer for Generated Machine Code
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

//-------------------------------------------------------------------------------
//  Label tracking (for remap and remove)
//-------------------------------------------------------------------------------
enum { LABEL_COUNT = 50 };
int curLabelIndex;

typedef struct {
    Label *label;
    Instr *instr;
    bool canRemap;      // can be remapped to another label?  (duplicate)
    bool canRemove;     // can be removed?  (unused)

    Label *newLabel;
} LabelInfo;

LabelInfo labelSet[LABEL_COUNT];

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

void clearLabelInfo(int index) {
    labelSet[index].label = NULL;
    labelSet[index].instr = NULL;
    labelSet[index].newLabel = NULL;
}

void print_labelList(const char *name) {
    printf("Label List: %s\n", name);
    printf("   %-20s:  Addr  MNE  remap  newLabel\n", "");
    printf("------------------------------------------\n");
    for (int labelIndex = 0; (labelSet[labelIndex].label != NULL) && (labelIndex < LABEL_COUNT); labelIndex++) {
        LabelInfo labelInfo = labelSet[labelIndex];
        Label *curLabel = labelSet[labelIndex].label;
        bool hasNewLabel = (labelInfo.newLabel != NULL && labelInfo.canRemap);
        printf("   %-20s:  %4X  %4s  %5s  %s\n",
               curLabel->name,
               curLabel->location,
               getMnemonicStr(labelInfo.instr->mne),
               (labelInfo.canRemap ? "true" : "false"),
               hasNewLabel ? labelInfo.newLabel->name : "");
    }
    printf("\n");
}


/**
 * Find All Labels and generate re-mappings to remove redundant ones
 *
 * @local lastInstrNone = flag to track whether last "instruction" actually
 *                        had an instruction/opcode or whether it was only
 *                        a label/comment line
 *
 * @param curInstr
 */

void FindAllLabels(Instr *curInstr) {
    static bool lastInstrNone = false;
    static Label *lastLabel = NULL;

    // if current instruction has a label, track it
    if (curInstr->label != NULL) {
        labelSet[curLabelIndex].label = curInstr->label;
        labelSet[curLabelIndex].instr = curInstr;
        labelSet[curLabelIndex].canRemap = lastInstrNone;

        // if previous line(s) had no instruction,
        //   we can mark labels for consolidation
        if (lastInstrNone) {
            labelSet[curLabelIndex].newLabel = lastLabel;
        }
        curLabelIndex++;

        if (!lastInstrNone) lastLabel = curInstr->label;
        lastInstrNone = (curInstr->mne == MNE_NONE);
    }

    // if there was an instruction here, remember that
    if (curInstr->mne != MNE_NONE) lastInstrNone = false;
}

static int labelLocation = 0;
void RecalcAllLabelLocations(Instr *curInstr) {
    if (curInstr->label != NULL) {
        curInstr->label->location = labelLocation;
        curInstr->label->hasLocation = true;
    }
    labelLocation += getInstrSize(curInstr->mne, curInstr->addrMode);
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
    clearLabelInfo(curLabelIndex);                  // clear first entry
    WalkInstructions(&FindAllLabels, instrBlock);
    clearLabelInfo(curLabelIndex);                  // mark end of list
}

void OPT_RecalcAllLabelLocations(InstrBlock *instrBlock) {
    labelLocation = instrBlock->funcSym->location;
    WalkInstructions(&RecalcAllLabelLocations, instrBlock);
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

static int instrLocation = 0;
void OptimizeBranchJumps(Instr *curInstr) {
    Instr *instr2 = curInstr->nextInstr;
    if (instr2 == NULL) return;
    Instr *instr3 = instr2->nextInstr;
    if (instr3 == NULL) return;

    // check for sequence
    //     BRx label
    //     JMP label2
    // label:
    //
    // REPLACE with:
    //     BRx label2

    if (isBranch(curInstr->mne) && (curInstr->paramName != NULL)
        && (instr2->mne == JMP) && (instr2->paramName != NULL)
        && (curInstr->paramName != NULL) && (instr3->label != NULL)
            && (strcmp(curInstr->paramName, instr3->label->name)==0)) {

        printf("Optimizing branch at %4X\n", instrLocation);
        curInstr->mne = invertBranch(curInstr->mne);
        curInstr->paramName = instr2->paramName;
        curInstr->nextInstr = instr3;
    }

    instrLocation += getInstrSize(curInstr->mne, curInstr->addrMode);
}

void OPT_Loops(OutputBlock *curBlock) {
    instrLocation = curBlock->codeBlock->funcSym->location;
    WalkInstructions(&OptimizeBranchJumps, curBlock->codeBlock);
}


/**
 * MAIN OPTIMIZER
 */

void OPT_CodeBlock(OutputBlock *curBlock) {
    InstrBlock *instrBlock = curBlock->codeBlock;

    OPT_FindAllLabels(instrBlock);
    OPT_RecalcAllLabelLocations(instrBlock);
    print_labelList(curBlock->blockName);

    OPT_Loops(curBlock);

    OPT_Jumps(instrBlock);
    OPT_RemapLabelsInBlock(instrBlock);

    //--- TODO: remove the following as they don't really do anything of value
    //          now that OPT_RemapLabelsInBlock has been fixed
    //OPT_RemoveUnusedLabels(instrBlock);
}

void OPT_DecCmp(OutputBlock *curBlock) {
    InstrBlock *instrBlock = curBlock->codeBlock;

    WalkInstructions(&OptimizeDecCmpZero, instrBlock);
}

/**
 * Resize / relocate code blocks after optimizations have been performed
 */

static int recalcAddr = 0;
static int blockSize = 0;
static OutputBlock *lastBlock = NULL;

void RECALC_CodeBlockSize(Instr *curInstr) {

}

void OPT_RecalcCodeBlockSpace(OutputBlock *curBlock) {

    if (lastBlock != NULL) {
        // recalculate location based on last block
        if (lastBlock->blockSize != blockSize) {

        }
    }
    if (blockSize + recalcAddr > 0) {
        recalcAddr = lastBlock + blockSize;
    } else {
        recalcAddr = curBlock->blockAddr;
    }

    WalkInstructions(&RECALC_CodeBlockSize, curBlock->codeBlock);
}

//================================================

void OPT_RunOptimizer() {
    printf("Running optimizer...\n");

    // optimize labels - currently done twice to catch what's missed on the first go-round
    OB_WalkCodeBlocks(&OPT_CodeBlock);
    //OB_WalkCodeBlocks(&OPT_CodeBlock);

    //----------
    OB_WalkCodeBlocks(&OPT_DecCmp);

    OB_WalkCodeBlocks(&OPT_RecalcCodeBlockSpace);
}

