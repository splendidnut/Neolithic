//
//  Write Output module
//
//  --- Write Assembly Code to output file
//
// Created by pblackman on 6/14/2020.
//

#include <stdio.h>
#include <string.h>
#include "common/common.h"
#include "output/output_block.h"
#include "write_output.h"

static FILE *outputFile;

void WO_Init(FILE *outFile) {
    outputFile = outFile;

    // print preamble
    if (outputFile != stdout) {
        fprintf(outputFile, "\t\tprocessor 6502\n\n");
    }
}

void WO_Done() {

}

/**
 * Get the parameter string to use when outputting the ASM code for an instruction
 * @param instr
 * @return
 */
// PARAM_NORMAL,
//    PARAM_LO = 0x1,           <param
//    PARAM_HI = 0x2,           >param
//    PARAM_ADD = 0x4,          (param1 + param2)
//    PARAM_PLUS_ONE = 0x10     (param1 + param2 + 1)  -- special case
char *getParamStr(const Instr *instr) {
    bool isRel = (instr->addrMode == ADDR_REL);
    bool isPlusOne = (instr->paramExt & PARAM_PLUS_ONE);

    int lenParam1 = (instr->paramName != NULL) ? strlen(instr->paramName) : 10;
    int lenParam2 = (instr->param2 != NULL) ? strlen(instr->param2) : 0;

    // figure out which parameter to use (varName or offset)
    char *prefix = "";
    char *suffix = "";
    if ((!instr->usesVar) && isRel) prefix = "*+";

    if (instr->param2 || isPlusOne) {
        prefix = "[";
        suffix = isPlusOne ? "+1]" : "]";
    }

    char *paramStr = malloc(lenParam1 + lenParam2 + 10);  // 10 to given spacing for deliminators
    paramStr[0] = '\0';
    if (!instr->usesVar) {
        strcat(paramStr, prefix);
        strcat(paramStr, numToStr(instr->offset));
    } else {
        switch (instr->paramExt & 0x3) {
            case PARAM_LO: strcat(paramStr, "<"); break;
            case PARAM_HI: strcat(paramStr, ">"); break;
        }
        strcat(paramStr, prefix);
        strcat(paramStr, instr->paramName);
    }

    // append 2nd parameter
    if (instr->param2 && (instr->paramExt & PARAM_ADD)) {
        if (instr->param2[0] != '-') {
            strcat(paramStr, "+");
        }
        strcat(paramStr, instr->param2);
    }
    strcat(paramStr, suffix);
    return paramStr;
}

char *getOpExt(enum MnemonicCode mne, struct StAddressMode addressMode) {
    char *opExt = "";
    bool isJump = (mne == JMP) || (mne == JSR);
    if (addressMode.mode > ADDR_ACC) opExt = "   ";
    if ((addressMode.mode == ADDR_ABS) && !isJump) opExt = ".w ";
    return opExt;
}


void WO_OutputInstr(FILE *output, Instr *instr) {
    struct StAddressMode addressMode = getAddrModeSt(instr->addrMode);
    char *instrName = getMnemonicStr(instr->mne);

    // first print any label
    if (instr->label != NULL) {
        fprintf(output, "%s:\n", instr->label->name);
    }

    char outStr[128], instrBuf[40];
    if (addressMode.mode != ADDR_NONE) {
        //----------------------------------
        // process parameters
        char *paramStr = getParamStr(instr);
        char *opExt = getOpExt(instr->mne, addressMode);

        // print out instruction line
        char *instrParams = malloc(80);
        sprintf(instrParams, addressMode.format, paramStr);
        sprintf(instrBuf, "%s%s  %s", instrName, opExt, instrParams);
        free(instrParams);
        free(paramStr);
    } else {
        sprintf(instrBuf, "%s", instrName);
    }

    // append any comments
    if (instr->lineComment != NULL) {
        snprintf(outStr, 120, "\t%-32s\t;-- %s", instrBuf, instr->lineComment);
    } else {
        snprintf(outStr, 120, "\t%s", instrBuf);
    }
    fprintf(output, "%s\n", outStr);
}


void WO_StartOfBank() {
    fprintf(outputFile, "\n\n\tORG $0000\n\tRORG $F000\n");
}

void WO_EndOfBank() {
    // print footer
    fprintf(outputFile, "\n\n\tORG $0FF8\n\tRORG $FFF8\n");
    fprintf(outputFile, "\t.word  $0000\n");
    fprintf(outputFile, "\t.word  $0000\n");
    fprintf(outputFile, "\t.word  main\n");
    fprintf(outputFile, "\t.word  main\n");
    fprintf(outputFile, "\n\n;--- END OF PROGRAM\n\n");
}

void WO_Variable(const char *name, const char *value) {
    fprintf(outputFile, "%s = %s\n", name, value);
}


void WO_FuncSymTables(SymbolRecord *funcSym) {// add the symbol (local/param) tables to the output code
    SymbolExt* funcExt = funcSym->funcExt;
    SymbolTable *funcSymbols = funcExt->localSymbolSet;
    SymbolTable *funcParams = funcExt->paramSymbolSet;
    if (funcSymbols != NULL) {
        WO_PrintSymbolTable(funcSymbols, "Local");
    }
    if (funcParams != NULL) {
        WO_PrintSymbolTable(funcParams, "Parameter");
    }
}

void WO_CodeBlock(InstrBlock *instrBlock, FILE *output) {
    if (instrBlock == NULL) return;
    Instr *curOutInstr = instrBlock->firstInstr;
    while (curOutInstr != NULL) {
        WO_OutputInstr(output, curOutInstr);
        curOutInstr = curOutInstr->nextInstr;
    }
}

void WO_WriteBlockHeader(const OutputBlock *outputBlock, bool doOutputLabel) {
    fprintf(outputFile, ";------------------------------------------------------\n");
    fprintf(outputFile, ";--  %04X: %s\n", outputBlock->blockAddr, outputBlock->blockName);
    fprintf(outputFile, ";--  %04X (bytes)\n\n", outputBlock->blockSize);
    if (doOutputLabel) {
        fprintf(outputFile, "%s:\n", outputBlock->blockName);
    }
}

void WO_WriteCodeBlockHeader(const InstrBlock *instrBlock, const char *name) {
    fprintf(outputFile, ";------------------------------------------------------\n");
    fprintf(outputFile, ";--  %04X: %s\n", instrBlock->codeAddr, name);
    fprintf(outputFile, ";--  %04X (bytes)\n\n", instrBlock->codeSize);
}

void WO_WriteBlockFooter(char *name) {
    fprintf(outputFile, "\techo \"%-30s \", (*-%s),(%s),\"-\",(*-1)\n\n", name, name, name);
}


void WO_FunctionBlock(const OutputBlock *block) {
    char *funcName = block->codeBlock->blockName;
    WO_WriteCodeBlockHeader(block->codeBlock, funcName);
    fprintf(outputFile, " SUBROUTINE\n");
    if (block->codeBlock->funcSym != NULL) {
        WO_FuncSymTables(block->codeBlock->funcSym);
    }
    WO_CodeBlock(block->codeBlock, outputFile);
    WO_WriteBlockFooter(funcName);
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

            if (curSymbol->isLocal) {                       // locale function vars
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


//------------------------------------------------------------------------
//   Output static array data
//
//  Outputs list of values from valueNode (ListNode)

void WO_StaticArrayData(const OutputBlock *block) {
    WO_WriteBlockHeader(block, true);

    //-------------------------------------------------
    //  print out data, putting 8 bytes on a line

    bool isInt = getBaseVarSize(block->dataSym) > 1;
    char *dataTypeStr = isInt ? "word" : "byte";
    int vend = block->dataList->count - 1;

    int accum = 0;
    for (int vidx = 0; vidx < vend; vidx++) {
        if (accum == 0) {
            fprintf(outputFile, "\t.%s ", dataTypeStr);
        }
        char sepChar;
        if (accum < 7 && (vidx != vend - 1)) {
            sepChar = ',';
            accum++;
        } else {
            sepChar = '\n';
            accum = 0;
        }
        fprintf(outputFile, "%d%c", block->dataList->nodes[vidx + 1].value.num, sepChar);
    }
    WO_WriteBlockFooter(block->blockName);
    fprintf(outputFile, "\n\n");
}

void HandleSingleStructRecord(const List *dataList, SymbolTable *structSymTbl) {
    int symIndex = 1;
    SymbolRecord *structVar = structSymTbl->firstSymbol;
    while (structVar != NULL) {
        int varSize = getBaseVarSize(structVar);
        char *dataTypeStr = (varSize > 1) ? "word" : "byte";

        int value = dataList->nodes[symIndex].value.num;

        if ((varSize == 1) && (value & 0xff00)) {
            printf("ERROR: The value %d exceeds the size of %s (type: %s)\n", value, structVar->name, dataTypeStr);
        }
        fprintf(outputFile, "\t.%s %-5d\t\t;-- %s\n", dataTypeStr, value, structVar->name);

        symIndex++;
        structVar = structVar->next;
    }
}

void WO_StaticStructData(const OutputBlock *block) {
    SymbolRecord *structSym = block->dataSym->userTypeDef;
    SymbolTable *structSymTbl = structSym->funcExt->paramSymbolSet;

    WO_WriteBlockHeader(block, true);

    // TODO: Are we assuming data is in an array?  Yes.

    List *dataList = block->dataList;
    if (dataList->hasNestedList && isToken(dataList->nodes[0], PT_LIST)) {
        int index = 1;
        while (index < dataList->count) {
            HandleSingleStructRecord(dataList->nodes[index].value.list, structSymTbl);
            fprintf(outputFile,"\n");
            index++;
        }
    } else {
        // Handle a single record
        HandleSingleStructRecord(dataList, structSymTbl);
    }
    WO_WriteBlockFooter(block->blockName);

    fprintf(outputFile, "\n\n");
}

/**
 * Output all blocks
 */

void WO_WriteAllBlocks() {
    const OutputBlock *block = OB_getFirstBlock();

    WO_StartOfBank();
    while (block != NULL) {
        switch (block->blockType) {
            case BT_CODE:
                WO_FunctionBlock(block);
                break;
            case BT_DATA:
                WO_StaticArrayData(block);
                break;
            case BT_STRUCT:
                WO_StaticStructData(block);
                break;
        }
        block = block->nextBlock;
    }
    WO_EndOfBank();
}
