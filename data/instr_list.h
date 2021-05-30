//
// Created by admin on 5/23/2020.
//

#ifndef MODULE_INSTR_LIST_H
#define MODULE_INSTR_LIST_H

#include "cpu_arch/asm_code.h"
#include "data/labels.h"
#include "symbols.h"


/**
 *  InstrStruct - Structure of a single assembly instruction (6502 based)
 */
typedef struct InstrStruct {
    enum MnemonicCode mne;
    enum AddrModes addrMode;
    bool usesVar;
    int offset;
    const char *paramName;

    // parameter extensions (provide a bit more flexibility to the compiler)
    enum ParamExt paramExt;
    const char *param2;

    char *lineComment;
    Label *label;

    struct InstrStruct *prevInstr;
    struct InstrStruct *nextInstr;
} Instr;

typedef struct InstrBlockStruct {
    int codeAddr;                       // address where code starts
    int codeSize;                       // size of code
    char *blockName;
    struct SymbolRecordStruct *funcSym;              // if part of a function (for outputting local/param vars)
    struct InstrStruct *firstInstr;
    struct InstrStruct *curInstr;
    struct InstrStruct *lastInstr;
} InstrBlock;

//----------------------------------------------------
//  Public Functions
//
//   - Start an instruction block
//   - Add instruction to block
//   - Set code address of block
//   - Output the block to an Assembly file

extern InstrBlock* IB_StartInstructionBlock(char *name);
extern void IB_AddInstr(InstrBlock *curBlock, Instr *newInstr);
extern void IB_SetCodeAddr(InstrBlock *block, int codeAddr);

#endif //MODULE_INSTR_LIST_H
