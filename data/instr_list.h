//
// Created by admin on 5/23/2020.
//

#ifndef MODULE_INSTR_LIST_H
#define MODULE_INSTR_LIST_H

#include "cpu_arch/asm_code.h"
#include "data/labels.h"
#include "symbols.h"

#define DOES_INSTR_USES_VAR(instr) ((instr)->paramName != NULL)
#define NOT_INSTR_USES_VAR(instr) ((instr)->paramName == NULL)

/**
 *  InstrStruct - Structure of a single assembly instruction (6502 based)
 *
 *  TODO: Optimize this structure
 */
typedef struct InstrStruct {
    enum MnemonicCode mne;
    enum AddrModes addrMode;
    bool showCycles;            // TODO: maybe optimize this functionality later?
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

extern void printInstrListMemUsage();

extern InstrBlock* IB_StartInstructionBlock(char *name);
extern void IB_AddInstr(InstrBlock *curBlock, Instr *newInstr);

//----------------------------------------------
//  Interface for Instruction List meta data

extern void IL_ShowCycles();
extern void IL_HideCycles();
extern void IL_MoveToNextPage();

extern void IL_Init(int startCodeAddr);
extern void IL_Preload(const SymbolRecord *varSym);
extern void IL_Label(Label *label);
extern void IL_SetLineComment(const char *comment);
extern Instr* IL_AddComment(Instr *inInstr, char *comment);
extern void IL_AddCommentToCode(char *comment);
extern void IL_SetLabel(Label *label);
extern Label* IL_GetCurLabel();

extern int IL_GetCodeSize(InstrBlock *instrBlock);

extern Instr* IL_AddInstrS(enum MnemonicCode mne, enum AddrModes addrMode, const char *param1, const char *param2, enum ParamExt paramExt);
extern Instr* IL_AddInstrN(enum MnemonicCode mne, enum AddrModes addrMode, int ofs);
extern Instr* IL_AddInstrB(enum MnemonicCode mne);

#endif //MODULE_INSTR_LIST_H
