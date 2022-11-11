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
// Created by User on 7/26/2021.
//

#include <stdio.h>
#include <string.h>
#include "write_output.h"

static FILE *outputFile;
static SymbolTable *mainSymbolTable;
static struct BankLayout *mainBankLayout;

//---------------------------------------------------------
// Output Adapter API

void WriteDASM_Init(FILE *outFile, SymbolTable *mainSymTbl, struct BankLayout *bankLayout);
void WriteDASM_Done();
char* WriteDASM_getExt();
void WriteDASM_FunctionBlock(const OutputBlock *block);
void WriteDASM_StaticArrayData(const OutputBlock *block);
void WriteDASM_StaticStructData(const OutputBlock *block);
void WriteDASM_StartOfBlock(const OutputBlock *block);
void WriteDASM_EndOfBlock(const OutputBlock *block);

struct OutputAdapter DASM_Adapter =
        {
                &WriteDASM_Init,
                &WriteDASM_Done,
                &WriteDASM_getExt,
                &WriteDASM_FunctionBlock,
                &WriteDASM_StaticArrayData,
                &WriteDASM_StaticStructData,
                &WriteDASM_StartOfBlock,
                &WriteDASM_EndOfBlock
        };

//---------------------------------------------------------
//--- Internal functions

void WriteDASM_WriteBlockHeader(const OutputBlock *outputBlock, bool doOutputLabel) {
    fprintf(outputFile, ";------------------------------------------------------\n");
    fprintf(outputFile, ";--  %04X: %s\n", outputBlock->blockAddr, outputBlock->blockName);
    fprintf(outputFile, ";--  %04X (bytes)\n\n", outputBlock->blockSize);
    if (doOutputLabel) {
        fprintf(outputFile, "%s:\n", outputBlock->blockName);
    }
}

void WriteDASM_WriteCodeBlockHeader(const OutputBlock *outputBlock, const char *name) {
    fprintf(outputFile, ";------------------------------------------------------\n");
    fprintf(outputFile, ";--  %04X: %s\n", outputBlock->blockAddr, name);
    fprintf(outputFile, ";--  %04X (bytes)\n\n", outputBlock->blockSize);
}

void WriteDASM_WriteBlockFooter(char *name) {
    fprintf(outputFile, "\techo \"%-30s \", (*-%s),(%s),\"-\",(*-1)\n\n", name, name, name);
}

void WriteDASM_StartOfBank() {
    int bankStart = mainBankLayout->banks[0].memLoc;
    fprintf(outputFile, "\n\n\tORG $0000");             // TODO: use file offset from bank in BankLayout
    fprintf(outputFile,"\n\tRORG $%4X\n", bankStart);
}

void WriteDASM_EndOfBank() {
    struct BankDef curBank = mainBankLayout->banks[0];
    int bankStart = curBank.memLoc;
    int bankEnd = (bankStart + curBank.size) & 0xfff0 + 0x8;

    // print footer
    fprintf(outputFile, "\n\n\tORG $0FF8\n");       // TODO: NEED to fix
    fprintf(outputFile,"\tRORG $%4X\n", bankEnd);
    fprintf(outputFile, "\t.word  $0000\n");
    fprintf(outputFile, "\t.word  $0000\n");
    fprintf(outputFile, "\t.word  main\n");
    fprintf(outputFile, "\t.word  main\n");
    fprintf(outputFile, "\n\n;--- END OF PROGRAM\n\n");
}

//-------------------------------------------------------
//--  Code Block handling / Instruction outputting

/**
 * Get the parameter string to use when outputting the ASM code for an instruction
 *
 * @param instr - instruction to pull parameters from
 * @param paramStrBuffer - buffer to store parameter string
 */
// PARAM_NORMAL,
//    PARAM_LO = 0x1,           <param
//    PARAM_HI = 0x2,           >param
//    PARAM_ADD = 0x4,          (param1 + param2)
//    PARAM_PLUS_ONE = 0x10     (param1 + param2 + 1)  -- special case
void LoadParamStr(const Instr *instr, char *paramStrBuffer) {
    bool isRel = (instr->addrMode == ADDR_REL);
    bool isPlusOne = (instr->paramExt & PARAM_PLUS_ONE);

    // figure out which parameter to use (varName or offset)
    char *prefix = "";
    char *suffix = "";
    if (NOT_INSTR_USES_VAR(instr) && isRel) prefix = "*+";

    if (instr->param2 || isPlusOne) {
        prefix = "[";
        suffix = isPlusOne ? "+1]" : "]";
    }

    paramStrBuffer[0] = '\0';
    if (NOT_INSTR_USES_VAR(instr)) {
        strcat(paramStrBuffer, prefix);
        strcat(paramStrBuffer, numToStr(instr->offset));
    } else {
        switch (instr->paramExt & 0x3) {
            case PARAM_LO: strcat(paramStrBuffer, "<"); break;
            case PARAM_HI: strcat(paramStrBuffer, ">"); break;
        }
        strcat(paramStrBuffer, prefix);
        strcat(paramStrBuffer, instr->paramName);
    }

    // append 2nd parameter
    if (instr->param2 && (instr->paramExt & PARAM_ADD)) {
        if (instr->param2[0] != '-') {
            strcat(paramStrBuffer, "+");
        }
        strcat(paramStrBuffer, instr->param2);
    }
    strcat(paramStrBuffer, suffix);
}

char *getOpExt(enum MnemonicCode mne, struct StAddressMode addressMode) {
    char *opExt = "";
    bool isJump = (mne == JMP) || (mne == JSR);
    if (addressMode.mode > ADDR_ACC) opExt = "   ";
    if ((addressMode.mode == ADDR_ABS) && !isJump) opExt = ".w ";
    return opExt;
}


/**
 * Write out a single instruction to the output file
 *
 * TODO: Figure out a better way to generate the output string.
 *        The current method seems a bit wonky (using sprintf, snprintf, etc...)
 *
 * @param output
 * @param instr
 */
static int runningCycleCount = 0;
void WriteDASM_OutputInstr(FILE *output, Instr *instr) {
    struct StAddressMode addressMode = getAddrModeSt(instr->addrMode);
    char *instrName = getMnemonicStr(instr->mne);

    // first print any label
    if (instr->label != NULL) {
        fprintf(output, "%s:\n", instr->label->name);
    }

    //----------------------------------------------
    //  Generate instruction line with parameters

    char instrBuf[80];
    if (addressMode.mode != ADDR_NONE) {

        // process parameters
        char paramStrBuffer[80];
        LoadParamStr(instr, paramStrBuffer);
        char *opExt = getOpExt(instr->mne, addressMode);

        // print out instruction line
        char instrParams[80];
        sprintf(instrParams, addressMode.format, paramStrBuffer);
        sprintf(instrBuf, "%s%s  %s", instrName, opExt, instrParams);

    } else if (instr->mne == MNE_DATA) {
        sprintf(instrBuf, "%s $%02X", instrName, instr->offset);
    } else if (instr->mne == MNE_DATA_WORD) {
        sprintf(instrBuf, "%s $%04X", instrName, instr->offset);
    } else {
        sprintf(instrBuf, "%s", instrName);
    }

    //----------------------------------------
    // Append instruction cycles (if enabled)

    char newLineComment[100];
    if (instr->showCycles && (instr->mne != MNE_NONE) && (instr->mne != MNE_DATA)) {
        int cycleCount = getCycleCount(instr->mne, instr->addrMode);
        runningCycleCount += cycleCount;

        if (instr->lineComment == NULL) instr->lineComment = "";

        sprintf(newLineComment, ";%d [%d] -- %s", cycleCount, runningCycleCount, instr->lineComment);
    }
    // need to handle when not showing cycles... comments vs no-comments
    else if (instr->lineComment != NULL) {
        snprintf(newLineComment, 100, ";-- %s", instr->lineComment);
    } else {
        newLineComment[0] = '\0';   // no comment
    }

    if (!instr->showCycles) runningCycleCount = 0;

    //------------------------------------------------
    // Build final ASM line (with any potential comments/cycle counts)

    char outStr[128];
    if (newLineComment[0] != '\0') {
        snprintf(outStr, 120, "\t%-32s\t%s", instrBuf, newLineComment);
        outStr[119] = '\0'; // snprintf doesn't properly terminate buffer when formatted string hits limit
    } else {
        snprintf(outStr, 120, "\t%s", instrBuf);
    }
    fprintf(output, "%s\n", outStr);
}

void WriteDASM_CodeBlock(InstrBlock *instrBlock, FILE *output) {
    if (instrBlock == NULL) return;
    Instr *curOutInstr = instrBlock->firstInstr;
    while (curOutInstr != NULL) {
        WriteDASM_OutputInstr(output, curOutInstr);
        curOutInstr = curOutInstr->nextInstr;
    }
}

//---------------------------------------------------------
//  Symbol Table handling

void WO_PrintSymbolTable(SymbolTable *workingSymbolTable, char *symTableName) {
    SymbolRecord *curSymbol = workingSymbolTable->firstSymbol;
    if (curSymbol == NULL) return;

    //---- count symbols that are not functions (vars and consts separately)

    int countSymbols = 0;
    int constSymbolCount = 0;

    while (curSymbol != NULL) {
        if (!isFunction(curSymbol)) countSymbols++;
        if (isSimpleConst(curSymbol)) constSymbolCount++;
        curSymbol = curSymbol->next;
    }


    // ---- print all symbols if there are any
    if (countSymbols > 0) {
        curSymbol = workingSymbolTable->firstSymbol;

        fprintf(outputFile, " ;-- %s Variables\n", symTableName);
        while (curSymbol != NULL) {
            int loc = curSymbol->location;

            // make sure location is valid
            if (loc >= 0) {
                if (IS_LOCAL(curSymbol)
                    && !isSimpleConst(curSymbol)
                    && (!IS_ALIAS(curSymbol))) {                       // locale function vars
                    fprintf(outputFile, ".%-20s = $%02X\n",
                            curSymbol->name,
                            curSymbol->location);
                } else if ((loc < 256) &&
                           (!isFunction(curSymbol))) {              // Zeropage are definitely variables...
                    fprintf(outputFile, "%-20s = $%02X\n",
                            curSymbol->name,
                            curSymbol->location);
                } else if (HAS_SYMBOL_LOCATION(curSymbol) && !isFunction(curSymbol)
                           && !isSimpleConst(curSymbol) && !isArrayConst(curSymbol)) {
                    fprintf(outputFile, "%-20s = $%04X  ;-- flags: %04X \n",
                            curSymbol->name,
                            curSymbol->location,
                            curSymbol->flags);
                }
            }

            curSymbol = curSymbol->next;
        };
    }
    fprintf(outputFile, "\n");

    if (constSymbolCount > 0) {
        curSymbol = workingSymbolTable->firstSymbol;
        fprintf(outputFile, " ;-- Constants\n");
        while (curSymbol != NULL) {
            if (isSimpleConst(curSymbol)) {
                if (IS_LOCAL(curSymbol)) {
                    fprintf(outputFile, ".");
                }
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



void WriteDASM_FuncSymTables(SymbolRecord *funcSym) {// add the symbol (local/param) tables to the output code
    SymbolTable *funcSymbols = GET_LOCAL_SYMBOL_TABLE(funcSym);
    if (funcSymbols != NULL) {
        WO_PrintSymbolTable(funcSymbols, "Local");
    }
}

//----

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
        fprintf(outputFile, "\t.%s $%04X\t\t;-- %s\n", dataTypeStr, value, structVar->name);

        symIndex++;
        structVar = structVar->next;
    }
}


//---------------------------------------------------------

void WriteDASM_Init(FILE *outFile, SymbolTable *mainSymTbl, struct BankLayout *bankLayout) {
    outputFile = outFile;
    mainSymbolTable = mainSymTbl;
    mainBankLayout = bankLayout;

    // print preamble
    if (outputFile != stdout) {
        fprintf(outputFile, "\t\tprocessor 6502\n\n");
        WO_PrintSymbolTable(mainSymbolTable, "Main");
        WriteDASM_StartOfBank();
    }
}

void WriteDASM_Done() {
    WriteDASM_EndOfBank();
    fclose(outputFile);
}

char* WriteDASM_getExt() { return ".asm"; }


void WriteDASM_FunctionBlock(const OutputBlock *block) {
    char *funcName = block->codeBlock->blockName;
    WriteDASM_WriteCodeBlockHeader(block, funcName);
    fprintf(outputFile, " SUBROUTINE\n");
    if (block->codeBlock->funcSym != NULL) {
        WriteDASM_FuncSymTables(block->codeBlock->funcSym);
    }
    WriteDASM_CodeBlock(block->codeBlock, outputFile);
    WriteDASM_WriteBlockFooter(funcName);
}

void WriteDASM_StartOfBlock(const OutputBlock *block) {
    if ((block->blockAddr & 0xff) == 0) {
        fprintf(outputFile,"\talign 256\n");
    }
}

void WriteDASM_EndOfBlock(const OutputBlock *block) {

}


//------------------------------------------------------------------------
//   Output static array data
//
//  Outputs list of values from valueNode (ListNode)

void WriteDASM_StaticArrayData(const OutputBlock *block) {
    WriteDASM_WriteBlockHeader(block, true);

    //-------------------------------------------------
    //  print out data, putting 8 bytes on a line

    bool isInt = getBaseVarSize(block->dataSym) > 1;
    const char *dataTypeStr = isInt ? "word" : "byte";
    const char *fmtStr = isInt ? "$%04X%c" : "%d%c";
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
        fprintf(outputFile, fmtStr, block->dataList->nodes[vidx + 1].value.num, sepChar);
    }
    WriteDASM_WriteBlockFooter(block->blockName);
    fprintf(outputFile, "\n\n");
}

void WriteDASM_StaticStructData(const OutputBlock *block) {
    SymbolRecord *baseSymbol = block->dataSym;
    SymbolRecord *structSym = block->dataSym->userTypeDef;
    SymbolTable *structSymTbl = GET_STRUCT_SYMBOL_TABLE(structSym);

    WriteDASM_WriteBlockHeader(block, true);

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
    WriteDASM_WriteBlockFooter(block->blockName);

    fprintf(outputFile, "\n\n");
}
