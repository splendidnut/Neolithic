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
    InstrBlock *codeBlock;   // if code
    List *dataList;          // if data list
    SymbolRecord *dataSym;   // if data list

    // doubly-linked list (to make element swapping easier)
    struct SOutputBlock *prevBlock;
    struct SOutputBlock *nextBlock;
} OutputBlock;

typedef void (*ProcessBlockFunc)(OutputBlock *);    // pointer to block processing function

extern void OB_Init();
extern void OB_AddBlock(OutputBlock *newBlock);
extern OutputBlock *OB_AddCode(char *name, InstrBlock *codeBlock);
extern OutputBlock *OB_AddData(SymbolRecord *dataSym, char *name, List *dataList);
extern void OB_PrintBlockList();
extern const OutputBlock *OB_getFirstBlock();

extern void OB_WalkCodeBlocks(ProcessBlockFunc codeBlockFunc);

#endif //MODULE_OUTPUT_BLOCK_H
