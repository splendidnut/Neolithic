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
//  Output Block handling
//
//  Handles a list of output blocks which can contain code or data,
//  allowing the blocks to be easily rearranged.
//
//  This opens up opportunities for the compiler to be smarter:
//   - re-arrange and align code/data to avoid page-crossing penalties
//   - arrange linked code in appropriate banks to minimize bank-switching.
//
// Created by admin on 6/15/2020.
//

#include <stdlib.h>

#include "output_block.h"

static OutputBlock *firstBlock;
static OutputBlock *curBlock;
static OutputBlock *lastBlock;

static int curAddr;
static int blockCount;

void DEBUG_printFirstBlockPtr(char *funcName) {
    OutputBlock *block = (OutputBlock *)OB_getFirstBlock();
    printf("%s has OutputBlock PTR: %p\n", funcName, block);
}

void OB_Init() {
    firstBlock = NULL;
    curBlock = NULL;
    lastBlock = NULL;
    curAddr = 0;
    blockCount = 0;
}

void OB_AddBlock(OutputBlock *newBlock) {

    // add block to the linked list
    if (firstBlock == NULL) {
        firstBlock = newBlock;
    } else {
        curBlock->nextBlock = newBlock;
    }
    curBlock = newBlock;
    lastBlock = curBlock;
    blockCount++;
}

/**
 * Add a function / code block to the Output Block list
 * @param name
 * @param codeBlock - InstrBlock
 * @return
 */
OutputBlock *OB_AddCode(char *name, InstrBlock *codeBlock, int suggestedBank) {
    OutputBlock *newBlock = allocMem(sizeof(struct SOutputBlock));
    newBlock->nextBlock = NULL;
    newBlock->blockName = name;
    newBlock->blockAddr = curAddr;
    newBlock->blockSize = codeBlock->codeSize;
    curAddr += codeBlock->codeSize;
    newBlock->blockType = BT_CODE;
    newBlock->codeBlock = codeBlock;
    newBlock->bankNum = suggestedBank;

    OB_AddBlock(newBlock);
    return newBlock;
}

// TODO: this is a hack since the OutputBlock module maintains a separate address tracker
void OB_MoveToNextPage() {
    curAddr = (curAddr + 256) & 0xff00;
}

OutputBlock *OB_AddData(SymbolRecord *dataSym, List *dataList, int suggestedBank) {
    bool isInt = getBaseVarSize(dataSym) > 1;
    OutputBlock *newBlock = allocMem(sizeof(struct SOutputBlock));
    newBlock->nextBlock = NULL;
    newBlock->blockName = dataSym->name;
    newBlock->blockAddr = curAddr;
    newBlock->blockSize = (dataList->count - 1) * (isInt ? 2 : 1);
    newBlock->blockType = BT_DATA;
    newBlock->dataSym = dataSym;
    newBlock->dataList = dataList;
    newBlock->bankNum = suggestedBank;

    // if this is a user-defined type (struct)
    if ((dataSym->userTypeDef != NULL) && (isStruct(dataSym->userTypeDef))) {
        newBlock->blockType = BT_STRUCT;
        newBlock->blockSize = dataSym->userTypeDef->numElements * dataSym->numElements;
    }
    curAddr += newBlock->blockSize;

    OB_AddBlock(newBlock);
    return newBlock;
}

void OB_PrintBlockList() {
    OutputBlock *block = firstBlock;
    printf("%-32s  addr   end    size  bank\n", "Block Name");
    printf("-----------------------------------------------------------\n");
    while (block != NULL) {
        printf("%-32s  %04X - %04X   %04X   %02X\n",
                block->blockName,
                block->blockAddr,
               (block->blockAddr + block->blockSize - 1),
                block->blockSize,
                block->bankNum);
        block = block->nextBlock;
    }
}

const OutputBlock *OB_getFirstBlock() {
    return firstBlock;
}

/**
 * Walk thru the output block list and call codeBlockFunc() for
 *   each code block found
 *
 * (Used by code optimizers)
 *
 * @param codeBlockFunc
 */
void OB_WalkCodeBlocks(ProcessBlockFunc codeBlockFunc) {
    //DEBUG_printFirstBlockPtr("OB_WalkCodeBlocks");

    OutputBlock *block = (OutputBlock *)OB_getFirstBlock();
    while (block != NULL) {
        if (block->blockType == BT_CODE)
            codeBlockFunc(block);
        block = block->nextBlock;
    }
}



/**
 * Block arrangement - Figure out where all the code and data blocks will go
 *
 */
void OB_ArrangeBlocks() {

    //--------------------------------------
    // TODO: Remove debug code

    printf("\n\nArranging blocks\n----------------\nInitial layout:\n");
    OB_PrintBlockList();

    //--------------------------------------
    // Actually do the arrangement

    OutputBlock *block = (OutputBlock *)OB_getFirstBlock();
    while (block != NULL) {
        if (block->blockType == BT_DATA) {
            printSingleSymbol(stdout, block->dataSym);
        }
        block = block->nextBlock;
    }

    //--------------------------------------
    // TODO: Remove debug code

    printf("\n\nNew Layout:\n");
    OB_PrintBlockList();
}