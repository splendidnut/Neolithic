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

void WO_Init(char *projectName, enum OutputType outputType, SymbolTable *mainSymTbl) {
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

    char* outFileName = genFileName(projectName, outputAdapter->getExt());
    printf("Writing %s\n", outFileName);
    if (outputType == OUT_BIN) {
        outputFile = fopen(outFileName, "wb");
    } else {
        outputFile = fopen(outFileName, "w");
    }
    if (outputFile != NULL) {
        outputAdapter->init(outputFile, mainSymTbl);
    } else {
        printf("ERROR: Unable to open file for writing!\n");
    }
}

void WO_Done() {
    if (outputAdapter != NULL) {
        outputAdapter->done();
    }
    if (outputFile != NULL) {
        fclose(outputFile);
    }
}


/**
 * Output Symbol Table
 * @param workingSymbolTable
 */

void WO_PrintConstantSymbols(SymbolTable *workingSymbolTable) {
    SymbolRecord *curSymbol = workingSymbolTable->firstSymbol;

    int constSymbolCount = 0;
    while (curSymbol != NULL) {
        if (isSimpleConst(curSymbol)) {
            constSymbolCount++;
        }
        curSymbol = curSymbol->next;
    }
    if (constSymbolCount > 0) {
        curSymbol = workingSymbolTable->firstSymbol;
        fprintf(outputFile, " ;-- Constants\n");
        while (curSymbol != NULL) {
            if (isSimpleConst(curSymbol)) {
                fprintf(outputFile, "%-20s = $%02X  ;--%s\n",
                        curSymbol->name,
                        curSymbol->constValue,
                        curSymbol->constEvalNotes);
            }
            curSymbol = curSymbol->next;
        }
    }
    fprintf(outputFile, "\n");
}

void WO_PrintSymbolTable(SymbolTable *workingSymbolTable, char *symTableName) {
    SymbolRecord *curSymbol = workingSymbolTable->firstSymbol;

    //---- count symbols that are not functions
    int countSymbols = 0;
    while (curSymbol != NULL) {
        if (!isFunction(curSymbol)) countSymbols++;
        curSymbol = curSymbol->next;
    }

    // ---- print all symbols if there are any
    curSymbol = workingSymbolTable->firstSymbol;
    if ((curSymbol != NULL) && (countSymbols > 0)) {
        fprintf(outputFile, " ;-- %s Variables\n", symTableName);
        while (curSymbol != NULL) {
            int loc = curSymbol->location;

            if (IS_LOCAL(curSymbol)) {                       // locale function vars
                fprintf(outputFile, ".%-20s = $%02X\n",
                        curSymbol->name,
                        curSymbol->location);
            } else if (loc > 0 && loc < 256) {              // Zeropage are definitely variables...
                fprintf(outputFile, "%-20s = $%02X\n",
                        curSymbol->name,
                        curSymbol->location);
            } else if (curSymbol->hasLocation && !isFunction(curSymbol)
                        && !isSimpleConst(curSymbol) && !isArrayConst(curSymbol)) {
                fprintf(outputFile, "%-20s = $%04X  ;-- flags: %04X \n",
                        curSymbol->name,
                        curSymbol->location,
                        curSymbol->flags);
            }

            curSymbol = curSymbol->next;
        };
    }
    fprintf(outputFile, "\n");
    WO_PrintConstantSymbols(workingSymbolTable);
}


/**
 * Output all blocks
 */

void WO_WriteAllBlocks() {
    if (outputAdapter == NULL) return;

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
