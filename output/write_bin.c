//
// Created by User on 7/27/2021.
//

#include <stdio.h>
#include "write_output.h"

//#define DEBUG_WRITE_BIN

static FILE *outputFile;
static SymbolTable *mainSymbolTable;
static SymbolTable *funcSymbolTable;
static SymbolTable *paramSymbolTable;

//---------------------------------------------------------
// Output Adapter API

void WriteBIN_Init(FILE *outFile, SymbolTable *mainSymTbl);
void WriteBIN_Done();
char* WriteBIN_getExt();
void WriteBIN_FunctionBlock(const OutputBlock *block);
void WriteBIN_StaticArrayData(const OutputBlock *block);
void WriteBIN_StaticStructData(const OutputBlock *block);
void WriteBIN_StartOfBlock(const OutputBlock *block);
void WriteBIN_EndOfBlock(const OutputBlock *block);

struct OutputAdapter BIN_OutputAdapter =
        {
                &WriteBIN_Init,
                &WriteBIN_Done,
                &WriteBIN_getExt,
                &WriteBIN_FunctionBlock,
                &WriteBIN_StaticArrayData,
                &WriteBIN_StaticStructData,
                &WriteBIN_StartOfBlock,
                &WriteBIN_EndOfBlock
        };

//---------------------------------------------------------

unsigned char *binData;


void WriteBIN_Init(FILE *outFile, SymbolTable *mainSymTbl) {
    outputFile = outFile;
    mainSymbolTable = mainSymTbl;
    funcSymbolTable = NULL;
    paramSymbolTable = NULL;
    binData = malloc(65536);
    for_range(i, 0, 4095) { binData[i] = 0; }
}

void WriteBIN_Done() {
    fwrite(binData,4096,1,outputFile);
    free(binData);
}

char* WriteBIN_getExt() { return ".bin"; }

void WriteBIN_StartOfBlock(const OutputBlock *block) {

}

void WriteBIN_EndOfBlock(const OutputBlock *block) {
    Label *mainLabel = findLabel("main");
    binData[4092] = mainLabel->location & 0xff;
    binData[4093] = (mainLabel->location >> 8) & 0xff;
    binData[4094] = mainLabel->location & 0xff;
    binData[4095] = (mainLabel->location >> 8) & 0xff;
}

/**
 * Process parameter string of an instruction
 *   and return the value
 *
 * @param param
 * @return
 */
int getParamStringValue(const char *param, int paramPos) {
#ifdef DEBUG_WRITE_BIN
    printf("\tparam #%d: %s\n", paramPos, param);
#endif

    // attempt to lookup the param in the symbol tables
    SymbolRecord *paramSym = NULL;
    if (paramSymbolTable != NULL) {
        paramSym = findSymbol(paramSymbolTable, param);
    }
    if ((paramSym == NULL) && (funcSymbolTable != NULL)) {
        paramSym = findSymbol(funcSymbolTable, param + 1);
    }
    if (paramSym == NULL) {
        paramSym = findSymbol(mainSymbolTable, param);
    }

    // if param is a symbol, return location or value
    if (paramSym != NULL) {
        if (paramSym->hasLocation) {
            return paramSym->location;
        } else if (paramSym->hasValue){
            return paramSym->constValue;
        }
    }

    // attempt to process as label
    Label *codeLabel = findLabel(param);
    if (codeLabel != NULL) {
#ifdef DEBUG_WRITE_BIN
        printf("Using label\n");
#endif
        return codeLabel->location;
    }

    // must be a value
    return strToInt(param);
}

int getInstrParamValue(const Instr *curOutInstr) {
    // PARAM_NORMAL,
    //    PARAM_LO = 0x1,           <param
    //    PARAM_HI = 0x2,           >param
    //    PARAM_ADD = 0x4,          (param1 + param2)
    //    PARAM_PLUS_ONE = 0x10     (param1 + param2 + 1)  -- special case

    int paramValue;

    if (!curOutInstr->usesVar) {
        paramValue = curOutInstr->offset;
        if (curOutInstr->addrMode == ADDR_REL) {
            paramValue -= 3;
        }
    } else {
        if (curOutInstr->paramName == NULL) return 0;
        paramValue = getParamStringValue(curOutInstr->paramName, 1);
    }

    if (curOutInstr->paramExt == PARAM_HI) {
        paramValue = paramValue >> 8;
    }

    if (curOutInstr->paramExt & PARAM_ADD) {
        int secondParamValue = getParamStringValue(curOutInstr->param2, 2);
        paramValue += secondParamValue;
    }

    if (curOutInstr->paramExt & PARAM_PLUS_ONE) {
        paramValue++;
    }

    return paramValue;
}

void WriteBIN_PreprocessLabels(const InstrBlock *instrBlock, int blockAddr) {
    Instr *curOutInstr = instrBlock->firstInstr;
    while (curOutInstr != NULL) {
        // handle label, if this instruction has a label, keep track of where it is
        if (curOutInstr->label != NULL) {
            curOutInstr->label->location = blockAddr + 0xF000;
            curOutInstr->label->hasLocation = true;
        }

        // If this is an instruction, increment PC
        if (curOutInstr->mne != MNE_NONE) {
            struct StAddressMode addressMode = getAddrModeSt(curOutInstr->addrMode);
            blockAddr += addressMode.instrSize;
        }
        curOutInstr = curOutInstr->nextInstr;
    }
}

void WriteBIN_FunctionBlock(const OutputBlock *block) {
#ifdef DEBUG_WRITE_BIN
    printf("Writing %s code to %4X\n", block->blockName, block->blockAddr);
#endif

    int writeAddr = block->blockAddr;

    InstrBlock *instrBlock = block->codeBlock;
    if (instrBlock == NULL) return;

    funcSymbolTable = instrBlock->funcSym->funcExt->localSymbolSet;
    paramSymbolTable = instrBlock->funcSym->funcExt->paramSymbolSet;

    WriteBIN_PreprocessLabels(instrBlock, writeAddr);

    Instr *curOutInstr = instrBlock->firstInstr;
    while (curOutInstr != NULL) {

        // Lookup the opcode and write out
        enum AddrModes addrMode = curOutInstr->addrMode;
        struct StAddressMode addressMode = getAddrModeSt(addrMode);
        OpcodeEntry opcodeEntry = lookupOpcodeEntry(curOutInstr->mne, addrMode);

        if (curOutInstr->mne != MNE_NONE) {
            binData[writeAddr++] = (curOutInstr->mne != MNE_DATA) ? opcodeEntry.opcode : curOutInstr->offset;

            //DEBUG
            //printf("Outputting %2X\n", opcodeEntry.opcode);

            if (addrMode != ADDR_NONE) {
                int paramValue = getInstrParamValue(curOutInstr);
                if (addrMode == ADDR_REL) {
#ifdef DEBUG_WRITE_BIN
                    if (curOutInstr->usesVar) {
                        printf("Current Address: %4X\n", writeAddr);
                        printf("Parameter value is %4X\n", paramValue);
                    } else {
                        printf("Relative to Current Address: %4X\n", writeAddr);
                    }
#endif
                    if (curOutInstr->usesVar) {
                        paramValue -= writeAddr + 1;
                    } else {
                        paramValue += 1;
                    }
#ifdef DEBUG_WRITE_BIN
                    printf("Opcode parameter is %4X\n", paramValue);
#endif
                    binData[writeAddr++] = paramValue;
                } else if (addressMode.instrSize == 2) {
                    binData[writeAddr++] = paramValue & 0xff;
                } else if (addressMode.instrSize == 3) {
                    binData[writeAddr++] = paramValue & 0xff;
                    binData[writeAddr++] = (paramValue >> 8) & 0xff;
                }
            }
        }
        curOutInstr = curOutInstr->nextInstr;
    }
#ifdef DEBUG_WRITE_BIN
    printf("Wrote %d bytes\n", (writeAddr - block->blockAddr));
#endif
}

void WriteBIN_StaticArrayData(const OutputBlock *block) {
#ifdef DEBUG_WRITE_BIN
    printf("Writing %s array data to %4X\n", block->blockName, block->blockAddr);
#endif
    int writeAddr = block->blockAddr;
    bool isInt = getBaseVarSize(block->dataSym) > 1;

    for_range (vidx, 1, block->dataList->count) {
        int value = block->dataList->nodes[vidx].value.num;
        binData[writeAddr++] = value & 0xff;
        if (isInt) {
            binData[writeAddr++] = (value >> 8) & 0xff;
        }
    }
}

void WriteBIN_StaticStructData(const OutputBlock *block) {
#ifdef DEBUG_WRITE_BIN
    printf("Writing %s struct data to %4X\n", block->blockName, block->blockAddr);
#endif
}