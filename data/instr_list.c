//
// Created by admin on 5/23/2020.
//

#include <stdlib.h>
#include "instr_list.h"

static int instrCount = 0;

InstrBlock* IB_StartInstructionBlock(char *name) {
    InstrBlock *newBlock = allocMem(sizeof(struct InstrBlockStruct));
    newBlock->codeSize = 0;
    newBlock->blockName = name;
    newBlock->firstInstr = NULL;
    newBlock->curInstr = NULL;
    newBlock->lastInstr = NULL;

    instrCount = 0;
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


void IB_SetCodeAddr(InstrBlock *block, int codeAddr) {
    block->codeAddr = codeAddr;
}


void IB_Walk(InstrBlock *instrBlock) {
    if (instrBlock == NULL) return;
    Instr *curOutInstr = instrBlock->firstInstr;
    while (curOutInstr != NULL) {
        //WO_OutputInstr(curOutInstr);
        curOutInstr = curOutInstr->nextInstr;
    }
}
