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
#include <string.h>

#include "output_block.h"
#include "codegen/gen_common.h"
#include "data/bank_layout.h"

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

void OB_SetAddress(int newAddr) {
    curAddr = newAddr;
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

OutputBlock *OB_FindByName(char *blockNameToFind) {
    OutputBlock *block = firstBlock;
    while (block != NULL) {
        if (strncmp(blockNameToFind, block->blockName, SYMBOL_NAME_LIMIT) == 0) {
            return block;
        }
        block = block->nextBlock;
    }
    return NULL;
}

void OB_PrintBlockList() {
    OutputBlock *block = firstBlock;
    printf("%-32s  addr   end    size  bank  size (bytes)\n", "Block Name");
    printf("--------------------------------------------------------------------------\n");
    while (block != NULL) {
        printf("%-32s  %04X - %04X   %04X   %02X  %5d bytes\n",
                block->blockName,
                block->blockAddr,
               (block->blockAddr + block->blockSize - 1),
                block->blockSize,
                block->bankNum,
                block->blockSize);
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

//-------------------------------------------------------------------
//--- Track code address
//-
// TODO:  This is temporary functionality until the "relocatable code/data" blocks issue is solved
// TODO: Figure out how to do this differently to make blocks magically movable
// TODO:  This can probably be figured out later in the compile process (output layout step)

bool checkIfBlockFits(const OutputBlock *outputBlock, const SymbolRecord *symRec) {
    // check if code doesn't fit in bank

    // information about bank to check
    int bankNum = outputBlock->bankNum;
    int bankEnd = (bankNum * 4096) + 0xFF7;

    //---
    int blockEndAddr = (outputBlock->blockAddr + outputBlock->blockSize) - 1;
    if (blockEndAddr > bankEnd) {
        char errStr[128];
        sprintf(errStr, "Block %s ends at: %04X (bank %d)", symRec->name, blockEndAddr, outputBlock->bankNum);
        ErrorMessage("Code block doesn't fit in bank\n\t", errStr, symRec->astList->lineNum);
        return false;
    }
    return true;
}

void checkAndSaveBlockAddr(const OutputBlock *outputBlock, SymbolRecord *symRec) {

    if (checkIfBlockFits(outputBlock, symRec)) {

        // use bank number to lookup code offset within bank
        int bankCodeAddr = BL_getMachineAddr(outputBlock->bankNum) + outputBlock->blockAddr;

        setSymbolLocation(symRec, bankCodeAddr, SS_ROM);
    }
}

bool locateBlockDuringGen = true;

void GC_OB_AddCodeBlock(SymbolRecord *funcSym, int curBank) {
    OutputBlock *result = OB_AddCode(funcSym->name, funcSym->instrBlock, curBank);

    if (!locateBlockDuringGen) return;

    checkAndSaveBlockAddr(result, funcSym);
}


void GC_OB_AddDataBlock(SymbolRecord *varSymRec, int curBank) {
    OutputBlock *staticData = OB_AddData(varSymRec, varSymRec->astList, curBank);

    if (!locateBlockDuringGen) return;

    checkAndSaveBlockAddr(staticData, varSymRec);
}



void OB_BuildInitialLayout() {
    OutputBlock *block = (OutputBlock *)OB_getFirstBlock();
    while (block != NULL) {
        SymbolRecord *symbolRecord;
        switch (block->blockType) {
            case BT_STRUCT:
            case BT_DATA:  symbolRecord = block->dataSym;  break;
            case BT_CODE:  symbolRecord = block->codeBlock->funcSym;  break;
        }

        if (symbolRecord != NULL && checkIfBlockFits(block, symbolRecord)) {
            // NEW CODE: use bank number to lookup code offset within bank
            int bankCodeAddr = BL_getMachineAddr(block->bankNum) + block->blockAddr;

            setSymbolLocation(symbolRecord, bankCodeAddr, SS_ROM);
        }
        block = block->nextBlock;
    }

    printf("\n\nArranging blocks - Initial layout:\n");
    OB_PrintBlockList();
    printf("\n");
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
    printf("\n");

    //--------------------------------------
    // Actually do the arrangement

    OutputBlock *block = (OutputBlock *)OB_getFirstBlock();
    printSymbolHeader(stdout);
    while (block != NULL) {
        switch (block->blockType) {
            case BT_STRUCT:
            case BT_DATA:  printSingleSymbol(stdout, block->dataSym);  break;
            case BT_CODE:  printSingleSymbol(stdout, block->codeBlock->funcSym);  break;
        }
        block = block->nextBlock;
    }
}