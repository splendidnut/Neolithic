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
// Created by admin on 5/23/2020.
//

#include <stdlib.h>
#include <assert.h>
#include "instr_list.h"

//---------------------------------------------
//  Instruction List Memory allocator

static unsigned int instrMemoryUsed = 0;
static unsigned int instrMaxMemoryUsed = 0;
static unsigned int instrListCount = 0;
static unsigned int instrLargestChunk = 0;

void *INSTR_allocMem(unsigned int size) {
    if (size > instrLargestChunk) instrLargestChunk = size;
    instrMemoryUsed += size;
    if (instrMemoryUsed > instrMaxMemoryUsed) instrMaxMemoryUsed = instrMemoryUsed;
    instrListCount++;
    return malloc(size);
}

void INSTR_freeMem(List *mem) {
    instrMemoryUsed -= (sizeof (List) + (mem->size * sizeof(ListNode)));
    free(mem);
}

void printInstrListMemUsage() {
    printf("\nInstruction List objects: %d", instrListCount);
    printf("\nInstruction List largest object: %d bytes", instrLargestChunk);
    printf("\nInstruction List memory usage: %d  (max: %d)\n", instrMemoryUsed, instrMaxMemoryUsed);
}

//--------------------------------------------------------

static int instrCount = 0;
static InstrBlock *curBlock;

static char* curLineComment;

InstrBlock* IB_StartInstructionBlock(char *name) {
    InstrBlock *newBlock = allocMem(sizeof(struct InstrBlockStruct));
    newBlock->codeSize = 0;
    newBlock->blockName = name;
    newBlock->firstInstr = NULL;
    newBlock->curInstr = NULL;
    newBlock->lastInstr = NULL;

    instrCount = 0;
    curBlock = newBlock;
    return newBlock;
}

/**
 * Add instruction to list
 * @param newInstr
 */
void IB_AddInstr(InstrBlock *curBlock, Instr *newInstr) {
    if (curBlock == NULL) return;

    // now add to the block
    newInstr->prevInstr = NULL;
    newInstr->nextInstr = NULL;
    if (curBlock->firstInstr != NULL) {
        newInstr->prevInstr = curBlock->curInstr;
        curBlock->curInstr->nextInstr = newInstr;
    } else {
        curBlock->firstInstr = newInstr;
    }
    curBlock->curInstr = newInstr;
    curBlock->lastInstr = newInstr;
    instrCount++;
}


void IB_Walk(InstrBlock *instrBlock) {
    if (instrBlock == NULL) return;
    Instr *curOutInstr = instrBlock->firstInstr;
    while (curOutInstr != NULL) {
        //WO_OutputInstr(curOutInstr);
        curOutInstr = curOutInstr->nextInstr;
    }
}



//---------------------------------------------------
//   Instruction List handling

Label *curLabel;
bool showCycles = false;

InstrBlock* IB_GetCurrentBlock() {
    return curBlock;
}

void IB_CloseBlock() {
    curBlock = NULL;
}

Instr* startNewInstruction(enum MnemonicCode mne, enum AddrModes addrMode) {
    Instr *newInstr = INSTR_allocMem(sizeof(struct InstrStruct));
    newInstr->mne = mne;
    newInstr->addrMode = addrMode;
    newInstr->showCycles = showCycles;

    // handle label and comment first
    newInstr->label = curLabel;
    newInstr->lineComment = curLineComment;
    curLabel = NULL;
    curLineComment = NULL;

    return newInstr;
}

/**
 * Add a new instruction to the instruction list.
 * @param mne - mnemonic of the instruction
 * @param addrMode - address mode
 * @param paramName - parameter name (if needed)
 * @return pointer to new instruction node
 */
Instr* IL_AddInstrS(enum MnemonicCode mne, enum AddrModes addrMode, const char *param1, const char *param2, enum ParamExt paramExt) {
    assert(param1 != NULL);
    Instr *newInstr = startNewInstruction(mne, addrMode);
    newInstr->paramName = param1;
    newInstr->param2 = param2;
    newInstr->paramExt = paramExt;

    IB_AddInstr(curBlock, newInstr);
    return newInstr;
}

/**
 * Add a new instruction to the instruction list. (single parameter)
 * @param mne - mnemonic of the instruction
 * @param addrMode - address mode
 * @param paramName - parameter name (if needed)
 * @return pointer to new instruction node
 */
Instr* IL_AddInstrP(enum MnemonicCode mne, enum AddrModes addrMode, const char *param1, enum ParamExt paramExt) {
    assert(param1 != NULL);
    Instr *newInstr = startNewInstruction(mne, addrMode);
    newInstr->paramName = param1;
    newInstr->paramExt = paramExt;
    newInstr->param2 = NULL;

    IB_AddInstr(curBlock, newInstr);
    return newInstr;
}

/**
 * Add instruction with numeric parameter
 * @param mne       - mnemonic
 * @param addrMode  - address mode
 * @param ofs       - numeric parameter
 * @return
 */
Instr* IL_AddInstrN(enum MnemonicCode mne, enum AddrModes addrMode, int ofs) {
    Instr *newInstr = startNewInstruction(mne, addrMode);
    newInstr->offset = ofs;
    newInstr->paramName = NULL;
    newInstr->paramExt = PARAM_NORMAL;
    newInstr->param2 = NULL;

    IB_AddInstr(curBlock, newInstr);
    return newInstr;
}

/**
 * Add basic instruction - single byte op
 * @param mne
 * @return
 */
Instr* IL_AddInstrB(enum MnemonicCode mne) {
    Instr *newInstr = startNewInstruction(mne, ADDR_NONE);
    IB_AddInstr(curBlock, newInstr);
    return newInstr;
}


Instr* IL_AddLabel(Instr *inInstr, Label *label) {
    inInstr->label = label;
    return inInstr;
}

Instr* IL_AddComment(Instr *inInstr, char *comment) {
    inInstr->lineComment = comment;
    return inInstr;
}

void IL_SetLineComment(const char *comment) {
    curLineComment = (char *)comment;
}

void IL_AddCommentToCode(char *comment) {
    IL_AddComment(IL_AddInstrB(MNE_NONE), comment);
}

void IL_SetLabel(Label *label) {
    curLabel = label;
}

Label* IL_GetCurLabel() {
    return curLabel;
}

Instr* IL_GetCurrentInstr() {
    return curBlock->curInstr;
}

/**
 * Return the number of bytes a block of code requires
 *
 * This functions walks thru all the instructions in a block and sums their lengths together.
 *
 * @param instrBlock
 * @return number of bytes needed to store the block of instructions
 */
int IL_GetCodeSize(InstrBlock *instrBlock) {
    Instr *curInstr = instrBlock->firstInstr;
    int size = 0;
    while (curInstr != NULL) {
        int instrSize = getInstrSize(curInstr->mne, curInstr->addrMode);
        //printf("%d\n", instrSize);
        size += instrSize;
        curInstr = curInstr->nextInstr;
    }
    return size;
}


/**
 * Return the number of bytes a range of code instructions requires
 *
 * This functions walks thru all the instructions between 'startInstr'
 *   and 'endInstr' sums their lengths together.
 *
 *   NOTE:  startInstr and endInstr need to be from the same code block!
 *
 * @param instrBlock
 * @return number of bytes needed to store the block of instructions
 */

int IL_GetCodeSizeOfRange(Instr *startInstr, Instr *endInstr) {
    Instr *curInstr = startInstr;
    int size = 0;
    while ((curInstr != NULL) && (curInstr != endInstr)) {
        int instrSize = getInstrSize(curInstr->mne, curInstr->addrMode);
        //printf("%d\n", instrSize);
        size += instrSize;
        curInstr = curInstr->nextInstr;
    }
    return size;
}

void IL_ShowCycles() {
    showCycles = true;
}

void IL_HideCycles() {
    showCycles = false;
}

/**
 * Scan thru provided range of instructions and switch branches to be long
 *   (for each branch, invert branch and add jump)
 * @param startInstr
 * @param endInstr
 */
void IL_FixLongBranch(Instr *startInstr, Instr *endInstr) {
    Instr *nextInstr, *newInstr;
    Instr *curInstr = startInstr;
    while (curInstr != NULL) {

        // make sure to save next instruction
        nextInstr = curInstr->nextInstr;

        if (isBranch(curInstr->mne)) {

            // create new instruction to insert
            newInstr = startNewInstruction(JMP, ADDR_ABS);
            newInstr->paramName = curInstr->paramName;
            newInstr->paramExt = PARAM_NORMAL;
            newInstr->nextInstr = nextInstr;

            // change branch instruction (invert)
            switch (curInstr->mne) {
                case BCC: curInstr->mne = BCS; break;
                case BCS: curInstr->mne = BCC; break;
                case BEQ: curInstr->mne = BNE; break;
                case BNE: curInstr->mne = BEQ; break;
                case BPL: curInstr->mne = BMI; break;
                case BMI: curInstr->mne = BPL; break;
            }
            curInstr->paramName = NULL;
            curInstr->param2 = NULL;
            curInstr->offset = +5;

            // point to new instruction (inserted)
            curInstr->nextInstr = newInstr;
        }

        if (curInstr == endInstr) {
            curInstr = NULL;            // after we've processed endInstr, then we can exit
        } else {
            //--- move on to next instruction (after patch)
            curInstr = nextInstr;
        }
    }
}