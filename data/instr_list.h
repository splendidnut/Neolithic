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

extern void printInstrListMemUsage();

extern InstrBlock* IB_StartInstructionBlock(char *name);
extern void IB_AddInstr(InstrBlock *curBlock, Instr *newInstr);
extern InstrBlock* IB_GetCurrentBlock();
extern void IB_CloseBlock();

//----------------------------------------------
//  Interface for Instruction List meta data

extern void IL_ShowCycles();
extern void IL_HideCycles();

extern void IL_Init();
extern void IL_Preload(const SymbolRecord *varSym);
extern void IL_Label(Label *label);
extern void IL_SetLineComment(const char *comment);
extern Instr* IL_AddComment(Instr *inInstr, char *comment);
extern void IL_AddCommentToCode(char *comment);
extern void IL_SetLabel(Label *label);
extern Label* IL_GetCurLabel();
extern Instr* IL_GetCurrentInstr();

extern int IL_GetCodeSize(InstrBlock *instrBlock);
extern int IL_GetCodeSizeOfRange(Instr *startInstr, Instr *endInstr);
extern void IL_FixLongBranch(Instr *startInstr, Instr *endInstr);

extern Instr* IL_AddInstrS(enum MnemonicCode mne, enum AddrModes addrMode, const char *param1, const char *param2, enum ParamExt paramExt);
extern Instr* IL_AddInstrP(enum MnemonicCode mne, enum AddrModes addrMode, const char *param1, enum ParamExt paramExt);
extern Instr* IL_AddInstrN(enum MnemonicCode mne, enum AddrModes addrMode, int ofs);
extern Instr* IL_AddInstrB(enum MnemonicCode mne);

#endif //MODULE_INSTR_LIST_H
