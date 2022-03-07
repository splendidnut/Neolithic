//
//  Write Output module using the selected outputType
//
//  This module is mainly a wrapper around all the potential
//  output modules.
//
//
// Created by pblackman on 6/14/2020.
//

#include <stdio.h>
#include "common/common.h"
#include "output/output_block.h"
#include "write_output.h"

struct OutputAdapter *outputAdapter;
static FILE *outputFile;


/**
 * Output all blocks
 */

void WO_WriteAllBlocks() {
    const OutputBlock *block = OB_getFirstBlock();

    while (block != NULL) {
        outputAdapter->startWriteBlock(block);
        switch (block->blockType) {
            case BT_CODE:
                outputAdapter->writeFunctionBlock(block);
                break;
            case BT_DATA:
                outputAdapter->writeStaticArrayData(block);
                break;
            case BT_STRUCT:
                outputAdapter->writeStaticStructData(block);
                break;
        }
        outputAdapter->endWriteBlock(block);
        block = block->nextBlock;
    }
}



void WriteOutput(char *projectName, enum OutputType outputType, SymbolTable *mainSymTbl, struct BankLayout *bankLayout) {

    // Select output adapter
    switch (outputType) {
        case OUT_DASM:
            outputAdapter = &DASM_Adapter;
            break;
        case OUT_BIN:
            outputAdapter = &BIN_OutputAdapter;
            break;
        default:
            printf("ERROR: Missing output module!\n");
            return;
    }

    char *outFileName = genFileName(projectName, outputAdapter->getExt());
    printf("Writing %s\n", outFileName);

    if (outputType == OUT_BIN) {
        outputFile = fopen(outFileName, "wb");
    } else {
        outputFile = fopen(outFileName, "w");
    }

    if (outputFile == NULL) {
        printf("ERROR: Unable to open file for writing!\n");
        return;
    }

    outputAdapter->init(outputFile, mainSymTbl, bankLayout);

    WO_WriteAllBlocks();

    outputAdapter->done();
    fclose(outputFile);
}