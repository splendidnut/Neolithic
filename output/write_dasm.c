//
// Created by User on 7/26/2021.
//

#include <stdio.h>
#include <string.h>
#include "write_output.h"

static FILE *outputFile;
static SymbolTable *mainSymbolTable;

//---------------------------------------------------------
// Output Adapter API

void WriteDASM_Init(FILE *outFile, SymbolTable *mainSymTbl);
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

void WriteDASM_WriteCodeBlockHeader(const InstrBlock *instrBlock, const char *name) {
    fprintf(outputFile, ";------------------------------------------------------\n");
    fprintf(outputFile, ";--  %04X: %s\n", instrBlock->codeAddr, name);
    fprintf(outputFile, ";--  %04X (bytes)\n\n", instrBlock->codeSize);
}

void WriteDASM_WriteBlockFooter(char *name) {
    fprintf(outputFile, "\techo \"%-30s \", (*-%s),(%s),\"-\",(*-1)\n\n", name, name, name);
}

void WriteDASM_StartOfBank() {
    fprintf(outputFile, "\n\n\tORG $0000\n\tRORG $F000\n");
}

void WriteDASM_EndOfBank() {
    // print footer
    fprintf(outputFile, "\n\n\tORG $0FF8\n\tRORG $FFF8\n");
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


/**
 * Write out a single instruction to the output file
 *
 * TODO: Figure out a better way to generate the output string.
 *        The current method seems a bit wonky (using sprintf, snprintf, etc...)
 *
 * @param output
 * @param instr
 */
void WriteDASM_OutputInstr(FILE *output, Instr *instr) {
    struct StAddressMode addressMode = getAddrModeSt(instr->addrMode);
    char *instrName = getMnemonicStr(instr->mne);
    static int runningCycleCount = 0;

    // first print any label
    if (instr->label != NULL) {
        fprintf(output, "%s:\n", instr->label->name);
    }

    //----------------------------------------------
    //  Generate instruction line with parameters

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
    } else if (instr->mne == MNE_DATA) {
        sprintf(instrBuf, "%s $%2X", instrName, instr->offset);
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

/**
 * Output Symbol Table
 * @param workingSymbolTable
 */

void WriteDASM_PrintConstantSymbols(SymbolTable *workingSymbolTable) {
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

void WriteDASM_PrintSymbolTable(SymbolTable *workingSymbolTable, char *symTableName) {
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

            if (IS_LOCAL(curSymbol)
                    && !isSimpleConst(curSymbol)
                    && (!IS_ALIAS(curSymbol))) {                       // locale function vars
                fprintf(outputFile, ".%-20s = $%02X\n",
                        curSymbol->name,
                        curSymbol->location);
            } else if (loc > 0 && loc < 256) {              // Zeropage are definitely variables...
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

            curSymbol = curSymbol->next;
        };
    }
    fprintf(outputFile, "\n");
    WriteDASM_PrintConstantSymbols(workingSymbolTable);
}

void WriteDASM_FuncSymTables(SymbolRecord *funcSym) {// add the symbol (local/param) tables to the output code
    SymbolExt* funcExt = funcSym->funcExt;
    SymbolTable *funcSymbols = funcExt->localSymbolSet;
    SymbolTable *funcParams = funcExt->paramSymbolSet;
    if (funcSymbols != NULL) {
        WriteDASM_PrintSymbolTable(funcSymbols, "Local");
    }
    if (funcParams != NULL) {
        WriteDASM_PrintSymbolTable(funcParams, "Parameter");
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
        fprintf(outputFile, "\t.%s %-5d\t\t;-- %s\n", dataTypeStr, value, structVar->name);

        symIndex++;
        structVar = structVar->next;
    }
}


//---------------------------------------------------------

void WriteDASM_Init(FILE *outFile, SymbolTable *mainSymTbl) {
    outputFile = outFile;
    mainSymbolTable = mainSymTbl;

    // print preamble
    if (outputFile != stdout) {
        fprintf(outputFile, "\t\tprocessor 6502\n\n");
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
    WriteDASM_WriteCodeBlockHeader(block->codeBlock, funcName);
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
    WriteDASM_WriteBlockFooter(block->blockName);
    fprintf(outputFile, "\n\n");
}

void WriteDASM_StaticStructData(const OutputBlock *block) {
    SymbolRecord *structSym = block->dataSym->userTypeDef;
    SymbolTable *structSymTbl = structSym->funcExt->paramSymbolSet;

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
