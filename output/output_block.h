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
// Created by admin on 6/15/2020.
//

#ifndef MODULE_OUTPUT_BLOCK_H
#define MODULE_OUTPUT_BLOCK_H

#include <stdbool.h>
#include <stdlib.h>

#include "data/instr_list.h"
#include "data/syntax_tree.h"

typedef enum {BT_CODE, BT_DATA, BT_STRUCT} BlockType;

typedef struct SOutputBlock {
    int blockAddr;
    int blockSize;
    char *blockName;
    int bankNum;            // which bank it's in

    BlockType blockType;
    SymbolRecord *dataSym;   // symbol that this block represents... (need to rename)
    union {
        InstrBlock *codeBlock;
        List *dataList;          // if data list
    };

    struct SOutputBlock *nextBlock;
} OutputBlock;

typedef void (*ProcessBlockFunc)(OutputBlock *);    // pointer to block processing function

extern void OB_Init();
extern void OB_AddBlock(OutputBlock *newBlock);
extern void OB_MoveToNextPage();
extern void OB_SetAddress(int newAddr);
extern OutputBlock *OB_AddCode(char *name, InstrBlock *codeBlock, int suggestedBank);
extern OutputBlock *OB_AddData(SymbolRecord *dataSym, List *dataList, int suggestedBank);
extern OutputBlock *OB_FindByName(char *blockNameToFind);
extern void OB_PrintBlockList();
extern const OutputBlock *OB_getFirstBlock();

extern void OB_WalkCodeBlocks(ProcessBlockFunc codeBlockFunc);
extern void OB_BuildInitialLayout();
extern void OB_ArrangeBlocks();

extern void GC_OB_AddCodeBlock(SymbolRecord *funcSym, int curBank);
extern void GC_OB_AddDataBlock(SymbolRecord *varSymRec, int curBank);
extern void GC_OB_AddLookupTable(SymbolRecord *varSymRec, int curBank);

#endif //MODULE_OUTPUT_BLOCK_H
