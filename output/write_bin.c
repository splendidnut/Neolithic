//
// Created by User on 7/27/2021.
//

#include <stdio.h>
#include "write_output.h"

//#define DEBUG_WRITE_BIN
//#define DEBUG_WRITE_BIN_OPCODE

static FILE *outputFile;
static SymbolTable *mainSymbolTable;
static SymbolTable *funcSymbolTable;

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
    binData = allocMem(65536);
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
    // attempt to lookup the param in the symbol tables
    SymbolRecord *paramSym = NULL;
    // TODO:  Fix this goofy-ness
    //   While working on moving params into the local symbol table, I stumbled upon this annoyance
    //  Due to local vars have '.', and params don't, we have to look for param vars separately
    if (funcSymbolTable != NULL) {
        paramSym = findSymbol(funcSymbolTable, param);
    }
    if ((paramSym == NULL) && (funcSymbolTable != NULL)) {
        paramSym = findSymbol(funcSymbolTable, param + 1);
    }
    if (paramSym == NULL) {
        paramSym = findSymbol(mainSymbolTable, param);
    }

    // if param is a symbol, return location or value
    if (paramSym != NULL) {
        if (HAS_SYMBOL_LOCATION(paramSym)) {
            return paramSym->location;
        } else if (paramSym->hasValue){
            return paramSym->constValue;
        }
    }

    // attempt to process as label
    Label *codeLabel = findLabel(param);
    if (codeLabel != NULL) {
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

    if (NOT_INSTR_USES_VAR(curOutInstr)) {
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

    funcSymbolTable = GET_LOCAL_SYMBOL_TABLE(instrBlock->funcSym);

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
#ifdef DEBUG_WRITE_BIN_OPCODE
            printf("%04X: Outputting %2X  (%02X, %s)\n",
                   writeAddr, opcodeEntry.opcode, curOutInstr->mne, addressMode.name);
#endif
            if (addrMode != ADDR_NONE) {
                int paramValue = getInstrParamValue(curOutInstr);
                if (addrMode == ADDR_REL) {
                    if (DOES_INSTR_USES_VAR(curOutInstr)) {
                        paramValue -= writeAddr + 1;
                    } else {
                        paramValue += 1;
                    }
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

/**
 * Write out the data to a single structure record
 *
 * @param structSymTbl  - structure of the data
 * @param writeAddr     - where in the output stream to write the data
 * @param dataList      -
 */
void WriteBIN_WriteStructRecordData(SymbolTable *structSymTbl, int writeAddr, const List *dataList) {
    int symIndex = 1;
    SymbolRecord *structVar = structSymTbl->firstSymbol;
    while (structVar != NULL) {
        int varSize = getBaseVarSize(structVar);
        char *dataTypeStr = (varSize > 1) ? "word" : "byte";

        int value = dataList->nodes[symIndex].value.num;

        if ((varSize == 1) && (value & 0xff00)) {
            printf("ERROR: The value %d exceeds the size of %s (type: %s)\n", value, structVar->name, dataTypeStr);
        }

        binData[writeAddr++] = value & 0xff;
        if (varSize == 2) {
            binData[writeAddr++] = (value >> 8) & 0xff;
        }
        symIndex++;

        structVar = structVar->next;
    }
}

void WriteBIN_StaticStructData(const OutputBlock *block) {
#ifdef DEBUG_WRITE_BIN
    printf("Writing %s struct data to %4X\n", block->blockName, block->blockAddr);
#endif
    SymbolRecord *structSym = block->dataSym->userTypeDef;
    SymbolTable *structSymTbl = GET_STRUCT_SYMBOL_TABLE(structSym);

    int writeAddr = block->blockAddr;
    List *dataList = block->dataList;
    if (isArray(block->dataSym)) {
        int structSize = calcVarSize(structSym);
        for_range(index, 1, block->dataList->count) {
            WriteBIN_WriteStructRecordData(structSymTbl, writeAddr, dataList->nodes[index].value.list);
            writeAddr += structSize;
        }
    } else {
        WriteBIN_WriteStructRecordData(structSymTbl, writeAddr, dataList);
    }
}