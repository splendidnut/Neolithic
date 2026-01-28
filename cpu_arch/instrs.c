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
//  Instruction List handling
//
// Created by admin on 4/6/2020.
//
//
//   TODO:  There appears to be cross references between instrs.* and instr_list.*  NEED TO FIX!

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/common.h"
#include "instrs.h"

//#define DEBUG_INSTRS


//--------------------------------------------------------
// Variables and Constants for Register-Use tracking

const LastRegisterUse REG_USED_FOR_NOTHING = {LW_NONE, 0, NULL};

LastRegisterUse lastUseForAReg;
LastRegisterUse lastUseForXReg;
LastRegisterUse lastUseForYReg;
LastRegisterUse lastStoredAReg;

//--------------------------------------------------
// Variables for tracking temporary indexing

const char *tempVarName = "0x80";
char *curCachedIndexVar;


//------------------------------------------------------------------------------
//--- Initialization of Instruction List / Instruction Code Generator

void IL_Init() {
    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
    lastUseForXReg = REG_USED_FOR_NOTHING;
    lastUseForYReg = REG_USED_FOR_NOTHING;

    curCachedIndexVar = "";

    IL_SetLineComment(NULL);
}

//-----------------------------------------------------------------------------
//---  System Init Code

// Atari 2600:
//      Need to reset the stack (Atari 2600 specific)
// TODO: move into machine specific module.

void ICG_SystemInitCode() {
    IL_AddInstrB(CLD);
    IL_AddComment(
            IL_AddInstrN(LDX, ADDR_IMM, 0xFF), "Initialize the Stack");
    IL_AddInstrB(TXS);
}



//-----------------------------------------------------------------------------
//  Function support

//static InstrBlock* curBlock;

/**
 * ICG_StartOfFunction - Prepares the Instruction Code Generator for the start of a new function block
 *
 * Creates a new instruction block and resets register trackers
 *
 * NOTE:  For main() function:  adds init code.
 *
 * @param funcLabel
 * @param funcSym
 * @return address where function starts
 */
void ICG_StartOfFunction(Label *funcLabel, SymbolRecord *funcSym) {
    InstrBlock *curBlock = IB_StartInstructionBlock(funcLabel->name);
    curBlock->funcSym = funcSym;

    IL_Label(funcLabel);

    // reset register trackers
    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
    lastUseForYReg = REG_USED_FOR_NOTHING;

    // main function needs to run init code
    if (isMainFunction(curBlock->funcSym)) {
        ICG_SystemInitCode();
    }
}

InstrBlock* ICG_EndOfFunction() {
    InstrBlock *funcInstrBlock = IB_GetCurrentBlock();//curBlock;

    // All functions except main() will need a RTS at the end.
    //    Add the RTS if it's not already there
    if (!isMainFunction(funcInstrBlock->funcSym) != 0) {
        if ((funcInstrBlock->curInstr == NULL) || (funcInstrBlock->curInstr->mne != RTS)) ICG_Return();
    }

    // save code size of function (to allow arrangement)
    int codeSize = IL_GetCodeSize(funcInstrBlock);
    funcInstrBlock->codeSize = codeSize;

    // reset current block
    IB_CloseBlock();
    //curBlock = NULL;

    return funcInstrBlock;
}


//================================================================================


void Dbg_IL_ClearOnUpdate(const char *varName, char *clearInfo) {
    sprintf(clearInfo, "Clear info: %s\t", varName);
    if (lastUseForAReg.loadedWith == LW_VAR) {
        strcat(clearInfo, "\tA: ");
        strcat(clearInfo, lastUseForAReg.varSym->name);
    }
    if (lastUseForXReg.loadedWith == LW_VAR) {
        strcat(clearInfo, "\tX: ");
        strcat(clearInfo, lastUseForXReg.varSym->name);
    }
    if ((lastUseForYReg.loadedWith == LW_VAR) || (lastUseForYReg.loadedWith == LW_VAR_X2)) {
        strcat(clearInfo, "\tY: ");
        strcat(clearInfo, lastUseForYReg.varSym->name);
    }
}

void IL_ClearCachedIndex() {
    curCachedIndexVar = "";
}

// clear out appropriate register tracker on var update
void IL_ClearOnUpdate(const SymbolRecord *varSym) {
    char *clearInfo = allocMem(120);
    char *varName = varSym->name;

    clearInfo[0] = 0;   // make sure it's initialized
#ifdef DEBUG_INSTRS
    Dbg_IL_ClearOnUpdate(varName, clearInfo);
#endif

    if ((lastUseForAReg.loadedWith == LW_VAR)
        && (strncmp(varName, lastUseForAReg.varSym->name, SYMBOL_NAME_LIMIT) == 0)) {
        lastUseForAReg = REG_USED_FOR_NOTHING;
        strcat(clearInfo, "\tA Cleared");
    }
    if ((lastStoredAReg.loadedWith == LW_VAR)
        && (strncmp(varName, lastStoredAReg.varSym->name, SYMBOL_NAME_LIMIT) == 0)) {
        lastStoredAReg = REG_USED_FOR_NOTHING;
        strcat(clearInfo, "\tA (stored) Cleared");
    }

    if ((lastUseForXReg.loadedWith == LW_VAR)
        && (strncmp(varName, lastUseForXReg.varSym->name, SYMBOL_NAME_LIMIT) == 0)) {
        lastUseForXReg = REG_USED_FOR_NOTHING;
        strcat(clearInfo, "\tX Cleared");
    }

    if (((lastUseForYReg.loadedWith == LW_VAR) || (lastUseForYReg.loadedWith == LW_VAR_X2))
        && (strncmp(varName, lastUseForYReg.varSym->name, SYMBOL_NAME_LIMIT) == 0)) {
        lastUseForYReg = REG_USED_FOR_NOTHING;
        strcat(clearInfo, "\tY Cleared");
    }

#ifdef DEBUG_INSTRS
    IL_AddCommentToCode(clearInfo);
#else
    free(clearInfo);
#endif
}

void IL_Preload(const SymbolRecord *varSym) {
    char *name = (char *)varSym->name;
    enum VarHint hint = varSym->hint;
    switch (hint) {
        case VH_A_REG:
            lastUseForAReg.loadedWith = LW_VAR;
            lastUseForAReg.varSym = varSym;
            break;
        case VH_X_REG:
            lastUseForXReg.loadedWith = LW_VAR;
            lastUseForXReg.varSym = varSym;
            break;
        case VH_Y_REG:
            lastUseForYReg.loadedWith = LW_VAR;
            lastUseForYReg.varSym = varSym;
            break;
        default:
            printf("Cannot preload '%s' using hint\n", name);
    }
}

//-------------------------------------------------------------------------------
//----  Miscellaneous supporting functions

void IL_Label(Label *label) {
    Label *curLabel = IL_GetCurLabel();
    if (curLabel != NULL && !(label->hasBeenReferenced)) {
        linkToLabel(label, curLabel);
    } else {
        if (label->hasBeenReferenced
               && ((curLabel != NULL) && (curLabel->hasBeenReferenced))) {

            // -- insert a dummy instruction to preserve label until we can consolidate
            IL_AddInstrN(MNE_NONE, ADDR_NONE, 0);
        }
        IL_SetLabel(label);
    }

    // labels kill any knowledge of what's in the registers
    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
    lastUseForYReg = REG_USED_FOR_NOTHING;
}


void ICG_Tag(char regName, SymbolRecord *varSym) {
    LastRegisterUse registerUse;
    registerUse.loadedWith = LW_VAR;
    registerUse.varSym = varSym;
    switch (regName) {
        case 'A': lastUseForAReg = registerUse; break;
        case 'X': lastUseForXReg = registerUse; break;
        case 'Y': lastUseForYReg = registerUse; break;
        default:break;
    }
}

bool ICG_IsCurrentTag(char regName, SymbolRecord *varSym) {
    LastRegisterUse registerUse;
    switch (regName) {
        case 'A': registerUse = lastUseForAReg; break;
        case 'X': registerUse = lastUseForXReg; break;
        case 'Y': registerUse = lastUseForYReg; break;
        default:return false;
    }
    return (registerUse.varSym == varSym);
}

bool ICG_isLastInstrReturn() {
    Instr *lastInstr = IL_GetCurrentInstr();
    return (lastInstr->mne == RTS);
}


//==============================================================================
//  Instruction Code Generators
//
//  The following functions add instructions to the instruction list
//
//  Each of these methods represents an intermediate instruction:
//     i.e.   ICG_LoadFromAddr = Load from address
//            ICG_LoadFromArray = Load from array
//


void ICG_LoadFromArray(const SymbolRecord *arraySymbol, int index,
                       enum SymbolType destType) {
    // const index, so add to address to read from
    lastUseForAReg = REG_USED_FOR_NOTHING;
    enum AddrModes addrMode = CALC_SYMBOL_ADDR_MODE(arraySymbol);
    if (destType == ST_PTR) {
        int ofs = (index * 2);
        IL_AddInstrS(LDA, addrMode, arraySymbol->name, intToStr(ofs), PARAM_ADD);
        IL_AddInstrS(LDX, addrMode, arraySymbol->name, intToStr(ofs+1), PARAM_ADD);
    } else {
        IL_AddInstrS(LDA, addrMode, arraySymbol->name, intToStr(index), PARAM_ADD);
    }
}

void ICG_LoadConst(int constValue, int size) {
    if ((lastUseForAReg.loadedWith == LW_CONST) && (lastUseForAReg.constValue == constValue)) return;

    IL_AddInstrN(LDA, ADDR_IMM, constValue & 0xff);
    if (size == 2) {
        IL_AddInstrN(LDX, ADDR_IMM, constValue >> 8);
    }

    // mark that we have a constant loaded
    lastUseForAReg.loadedWith = LW_CONST;
    lastUseForAReg.constValue = constValue;
}

//-------------------------------------------------------------------
//  Consolidate any accesses to Param variables here

void ICG_OpWithParamVar(enum MnemonicCode mne, const SymbolRecord *varRec) {
    IL_AddComment(
            IL_AddInstrB(TSX),
            "prepare to read param var");
    IL_AddInstrP(mne, ADDR_ABX, getVarName(varRec), PARAM_NORMAL);
}

//-------------------------------------------------------------------

void ICG_LoadByteVar(const SymbolRecord *varRec, int ofs) {
    const char *varName = getVarName(varRec);
    enum AddrModes addrMode = (ofs < 0x100 ? ADDR_ZP : ADDR_ABS);
    IL_AddInstrS(LDA, addrMode, varName, numToStr(ofs), PARAM_ADD);
    lastUseForAReg = REG_USED_FOR_NOTHING;
}

void ICG_LoadVar(const SymbolRecord *varRec) {
    // Check contents of A reg.
    if ((lastUseForAReg.loadedWith == LW_VAR) && (lastUseForAReg.varSym == varRec)) return;
    if ((lastStoredAReg.loadedWith == LW_VAR) && (lastStoredAReg.varSym == varRec)) {
        lastUseForAReg = lastStoredAReg;
        return;
    }

    const char *varName = getVarName(varRec);
    if (isConst(varRec)) {
        IL_AddInstrP(LDA, ADDR_IMM, varName, PARAM_LO);
        if (getBaseVarSize(varRec) == 2) {
            IL_AddInstrP(LDX, ADDR_IMM, varName, PARAM_HI);
        }
    } else if (IS_PARAM_VAR(varRec)) {
        switch (varRec->hint) {
            case VH_A_REG:      // already loaded?  TODO: check to see if var sym matches
                break;
            case VH_X_REG:
                IL_AddInstrB(TXA);
                break;
            case VH_Y_REG:
                IL_AddInstrB(TYA);
                break;
            default:
                ICG_OpWithParamVar(LDA, varRec);
        }
    } else {
        IL_AddInstrP(LDA, ADDR_ZP, varName, PARAM_NORMAL);
        if (getBaseVarSize(varRec) == 2) {
            IL_AddInstrP(LDX, ADDR_ZP, varName, PARAM_PLUS_ONE);
        }
    }

    lastStoredAReg.loadedWith = LW_NONE;
    lastUseForAReg.loadedWith = LW_VAR;
    lastUseForAReg.varSym = varRec;
}

bool isLastYUse(const SymbolRecord *varSym, int size) {
    enum LastRegisterUseType regUseType = size == 2 ? LW_VAR_X2 : LW_VAR;
    return ((lastUseForYReg.loadedWith == regUseType)
            && (strncmp(lastUseForYReg.varSym->name, varSym->name, SYMBOL_NAME_LIMIT) == 0));
}

void ICG_LoadIndexVar(const SymbolRecord *varSym, int size) {
    const char *varName = getVarName(varSym);

    if (strncmp(varName, curCachedIndexVar, SYMBOL_NAME_LIMIT)==0) {
        IL_AddComment(
            IL_AddInstrP(LDY, ADDR_ZP, tempVarName, PARAM_NORMAL), "load cached index");
        lastUseForYReg = REG_USED_FOR_NOTHING;
        return;
    }

    bool needToLoad = !isLastYUse(varSym, size);

    if (needToLoad) {
        if (size == 2) {
            IL_AddComment(
                    IL_AddInstrP(LDA, ADDR_ZP, varName, PARAM_NORMAL), "load array index");
            IL_AddInstrN(ASL, ADDR_ACC, 0);
            IL_AddInstrB(TAY);
            lastUseForYReg.loadedWith = LW_VAR_X2;
            lastUseForAReg = REG_USED_FOR_NOTHING;
        } else {
            if (IS_PARAM_VAR(varSym)) {
                ICG_OpWithParamVar(LDY, varSym);
            } else {
                IL_AddComment(
                        IL_AddInstrP(LDY, ADDR_ZP, varName, PARAM_NORMAL), "load array index");
            }
            lastUseForYReg.loadedWith = LW_VAR;
        }
        lastUseForYReg.varSym = varSym;
    }
#ifdef DEBUG_INSTRS
    else {
        char *clearInfo = malloc(120);
        sprintf(clearInfo, "No need to load index for array access: %s", varSym->name);
        IL_AddCommentToCode(clearInfo);
    }
#endif
}

void ICG_SaveIndexVar(const SymbolRecord *varSym, int size) {
    const char *varName = getVarName(varSym);

    IL_AddComment(
            IL_AddInstrP(LDA, ADDR_ZP, varName, PARAM_NORMAL), "load array index");
    if (size == 2) {
        IL_AddInstrN(ASL, ADDR_ACC, 0);
    }
    IL_AddInstrP(STA, ADDR_ZP, tempVarName, PARAM_NORMAL);

    curCachedIndexVar = varName;
    lastUseForAReg = REG_USED_FOR_NOTHING;
}

void ICG_LoadAddr(const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);
    IL_AddInstrP(LDA, ADDR_IMM, varName, PARAM_LO);
    IL_AddInstrP(LDX, ADDR_IMM, varName, PARAM_HI);

    lastUseForAReg.loadedWith = LW_VAR;
    lastUseForAReg.varSym = varSym;
}

void ICG_LoadAddrPlusIndex(const SymbolRecord *varSym, unsigned char index) {
    const char *varName = getVarName(varSym);
    IL_AddInstrS(LDA, ADDR_IMM, varName, numToStr(index), PARAM_ADD + PARAM_LO);
    IL_AddInstrS(LDX, ADDR_IMM, varName, numToStr(index), PARAM_ADD + PARAM_HI);
    lastUseForAReg = REG_USED_FOR_NOTHING;
}

void ICG_LoadIndirect(const SymbolRecord *varSym, int destSize) {
    const char *varName = getVarName(varSym);
    if (destSize == 2) {
        IL_AddComment(
                IL_AddInstrB(INY),
                "load indirect - high byte first");
        IL_AddComment(
                IL_AddInstrP(LDA, ADDR_IY, varName, PARAM_NORMAL),
                "load from pointer location using index");
        IL_AddInstrB(TAX);
        IL_AddInstrB(DEY);
    }
    IL_AddComment(
            IL_AddInstrP(LDA, ADDR_IY, varName, PARAM_NORMAL),
            "load from pointer location using index");
    lastUseForAReg = REG_USED_FOR_NOTHING;
}

void ICG_LoadIndexed(const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);
    IL_AddComment(
            IL_AddInstrP(LDA, ADDR_ABY, varName, PARAM_NORMAL),
            "load from array using index");
    if (getBaseVarSize(varSym) > 1) {
        IL_AddInstrP(LDX, ADDR_ABY, varName, PARAM_NORMAL + PARAM_PLUS_ONE);
    }
    lastUseForAReg = REG_USED_FOR_NOTHING;
}

void ICG_LoadIndexedWithOffset(const SymbolRecord *varSym, int ofs, int varSize) {
    const char *varName = getVarName(varSym);
    IL_AddComment(
            IL_AddInstrS(LDA, ADDR_ABY, varName, numToStr(ofs), PARAM_NORMAL + PARAM_ADD),
            "load from array using index with offset");

    if (varSize == 2) {
        IL_AddInstrS(LDX, ADDR_ABY, varName, numToStr(ofs+1), PARAM_NORMAL + PARAM_ADD);
    }
    lastUseForAReg = REG_USED_FOR_NOTHING;
}

void ICG_LoadPropertyVar(const SymbolRecord *structSym, const SymbolRecord *propertySym) {
    const char *structName = getVarName(structSym);
    enum AddrModes addrMode = CALC_SYMBOL_ADDR_MODE(structSym);
    unsigned char propertyOfs = (propertySym->location & 0xff);

    IL_AddComment(
            IL_AddInstrS(LDA, addrMode, structName, numToStr(propertyOfs), PARAM_NORMAL + PARAM_ADD),
            getStructRefComment("load structure ref", structName, propertySym->name));

    if (getBaseVarSize(propertySym) == 2) {
        IL_AddInstrS(LDX, addrMode, structName, numToStr(propertyOfs+1), PARAM_NORMAL + PARAM_ADD);
    }
    lastUseForAReg = REG_USED_FOR_NOTHING;
}

/**
 * Load register with const value (used for function arguments - HINT system)
 * @param destReg
 * @param ofs
 */
void ICG_LoadRegConst(const char destReg, int ofs) {
    switch (destReg) {
        case 'A':
            if (!((lastUseForAReg.loadedWith == LW_CONST) && (lastUseForAReg.constValue == ofs))) {
                IL_AddInstrN(LDA, ADDR_IMM, ofs);
                lastUseForAReg.loadedWith = LW_CONST;
                lastUseForAReg.constValue = ofs;
            }
            break;
        case 'X':
            IL_AddInstrN(LDX, ADDR_IMM, ofs);
            break;
        case 'Y':
            if (!((lastUseForYReg.loadedWith == LW_CONST) && (lastUseForYReg.constValue == ofs))) {
                IL_AddInstrN(LDY, ADDR_IMM, ofs);
                lastUseForYReg.loadedWith = LW_CONST;
                lastUseForYReg.constValue = ofs;
            }
            break;
    }
}

void ICG_LoadRegVar(const SymbolRecord *varSym, char destReg) {
    const char *varName = getVarName(varSym);
    enum MnemonicCode mne;
    switch (destReg) {
        case 'A': mne = LDA; break;
        case 'X': mne = LDX; break;
        case 'Y': mne = LDY; break;
        default:break;
    }
    enum AddrModes addrMode = isConst(varSym) ? ADDR_IMM : CALC_SYMBOL_ADDR_MODE(varSym);
    IL_AddInstrP(mne, addrMode, varName, PARAM_NORMAL);

    if (destReg == 'A') {
        lastUseForAReg = REG_USED_FOR_NOTHING;
    }
}

void ICG_LoadPointerAddr(const SymbolRecord *varSym) {
    IL_SetLineComment(varSym->name);

    IL_AddComment(
        IL_AddInstrP(LDA, ADDR_IMM, getVarName(varSym), PARAM_LO),
        "ICG_LoadPointerAddr");
    ICG_PushAcc();
    IL_AddInstrP(LDA, ADDR_IMM, getVarName(varSym), PARAM_HI);
    ICG_PushAcc();

    // mark that we have nothing loaded, since we pushed the DATA
    lastUseForAReg.loadedWith = LW_NONE;
}

void ICG_AdjustStack(int ofs) {
    IL_AddInstrB(TSX);
    for (int cnt=0; cnt < ofs; cnt++) {
        IL_AddInstrN(INX, ADDR_NONE, cnt);
    }
    IL_AddInstrB(TXS);
}

//--------------------------------------------------------------

void ICG_StoreToAddr(int ofs, int size) {
    enum AddrModes addrMode = (ofs < 0x100 ? ADDR_ZP : ADDR_ABS);
    IL_AddInstrN(STA, addrMode, ofs);
    if (size == 2) {
        IL_AddInstrN(STX, addrMode, ofs + 1);
    }
}

void ICG_StoreVarOffset(const SymbolRecord *varSym, int ofs, int destSize) {
    const char *varName = getVarName(varSym);

    if (isPointer(varSym) && isStructDefined(varSym)) {
        ICG_LoadRegConst('Y', ofs);
        IL_AddInstrP(STA, ADDR_IY, varName, PARAM_NORMAL);
        if (destSize == 2) {
            IL_AddInstrB(TXA);
            IL_AddInstrB(INY);
            IL_AddInstrP(STA, ADDR_IY, varName, PARAM_NORMAL);
        }
    } else {
        enum AddrModes addrMode = CALC_SYMBOL_ADDR_MODE(varSym);
        enum ParamExt paramExt = ((addrMode == ADDR_ZP) ? PARAM_LO : PARAM_NORMAL);

        if (destSize == 2) {
            IL_AddInstrS(STX, addrMode, varName, numToStr(ofs), paramExt + PARAM_ADD + PARAM_PLUS_ONE);
        }
        IL_AddInstrS(STA, addrMode, varName, numToStr(ofs), paramExt + PARAM_ADD);
    }
}

/**
 * Store to a variable that is Indexed by the Y register
 * @param varSym
 */
void ICG_StoreVarIndexed(const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);

    /*if (isPointer(varSym)) {
        bool isInt = ((varSym->flags & ST_MASK) == ST_INT);
        IL_AddInstrP(STA, ADDR_IY, varName, PARAM_NORMAL);
        return;
    }*/

    IL_AddInstrP(STA, ADDR_ABY, varName, PARAM_NORMAL);
    if (getBaseVarSize(varSym) == 2) {
        enum AddrModes addrMode = CALC_SYMBOL_ADDR_MODE(varSym) + ADDR_Y;
        if (addrMode == ADDR_ZPY) {
            IL_AddInstrP(STX, addrMode, varName, PARAM_PLUS_ONE + PARAM_LO);
        } else {
            IL_AddInstrB(TXA);
            IL_AddInstrP(STA, addrMode, varName, PARAM_PLUS_ONE + PARAM_NORMAL);
        }
    }
}

/**
 * Store a pointer (value in A+X regs) to a variable that is Indexed by the Y register
 * @param varSym
 */
void ICG_StoreIndirect(const SymbolRecord *varSym, int dstSize) {
    const char *varName = getVarName(varSym);
    IL_AddInstrP(STA, ADDR_IY, varName, PARAM_NORMAL);
    if (dstSize == 2) {
        IL_AddInstrB(INY);
        IL_AddInstrB(TXA);
        IL_AddInstrP(STA, ADDR_IY, varName, PARAM_NORMAL);
        //-- clear out A+Y regs, since they've been modified
        lastUseForAReg = REG_USED_FOR_NOTHING;
        lastUseForYReg = REG_USED_FOR_NOTHING;
    }
}


void ICG_StoreVarSym(const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);

    bool isAregUnknown = (lastUseForAReg.loadedWith == LW_NONE);

    enum AddrModes addrMode = CALC_SYMBOL_ADDR_MODE(varSym);

    IL_ClearOnUpdate(varSym);
    IL_AddInstrP(STA, addrMode, varName, PARAM_NORMAL);
    if (getBaseVarSize(varSym) == 2) {
        IL_AddInstrP(STX, addrMode, varName, PARAM_PLUS_ONE);
    }

    if (isAregUnknown) {
        lastUseForAReg.loadedWith = LW_VAR;
        lastUseForAReg.varSym = varSym;
    }
    lastStoredAReg.loadedWith = LW_VAR;
    lastStoredAReg.varSym = varSym;
}

void ICG_StoreIndexedWithOffset(const SymbolRecord *varSym, int ofs, int varSize) {
    const char *varName = getVarName(varSym);
    IL_AddComment(
            IL_AddInstrS(STA, ADDR_ABY, varName, numToStr(ofs), PARAM_NORMAL + PARAM_ADD),
            "store to array using index with offset");
    if (varSize == 2) {
        IL_AddInstrB(TXA);
        IL_AddInstrS(STA, ADDR_ABY, varName, numToStr(ofs), PARAM_PLUS_ONE + PARAM_ADD);
    }
}

//---------------------------------------------------------

void ICG_Branch(enum MnemonicCode mne, const Label *label) {
    Label *branchLabel = (label->link != NULL) ? label->link : (Label *)label;  // use linked label if available
    addLabelRef(branchLabel);
    IL_AddInstrP(mne, ADDR_REL, branchLabel->name, PARAM_NORMAL);
}

void ICG_Not() {
    IL_AddInstrN(EOR, ADDR_IMM, 0xFF);
}

void ICG_NotBool() {
    IL_AddInstrN(EOR, ADDR_IMM, 0x01);
}

void ICG_Negate() {
    IL_AddInstrN(EOR, ADDR_IMM, 0xFF);
    IL_AddInstrB(CLC);
    IL_AddInstrN(ADC, ADDR_IMM, 1);
}

void ICG_PreOp(enum MnemonicCode preOp) {
    if (preOp != MNE_NONE) IL_AddInstrB(preOp);
}

void ICG_OpHighByte(enum MnemonicCode mne, const char *varName) {
    // Handle 16-bit ops
    switch (mne) {
        case ADC:
            IL_AddInstrN(BCC, ADDR_REL, +3);
            IL_AddInstrB(INX);
            break;
        case SBC:
            IL_AddInstrN(BCS, ADDR_REL, +3);
            IL_AddInstrB(DEX);
            break;
        case INC:
            if (varName != NULL) {
                IL_AddInstrN(BNE, ADDR_REL, +4);    // +4 for 2 byte op, +5 for 3 byte op
                IL_AddInstrP(mne, ADDR_ZP, varName, PARAM_PLUS_ONE);
            } else {
                IL_AddInstrN(BNE, ADDR_REL, +3);
                IL_AddInstrB(INX);
            }
            break;
        default: break;
    }
}

void ICG_OpWithConst(enum MnemonicCode mne, int num, int dataSize) {
    IL_AddInstrN(mne, ADDR_IMM, num);
    if (dataSize > 1) ICG_OpHighByte(mne, NULL);

    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
}

void ICG_OpWithVar(enum MnemonicCode mne, const SymbolRecord *varSym, int dataSize) {
    const char *varName = getVarName(varSym);
    if (isConst(varSym)) {
        IL_AddInstrP(mne, ADDR_IMM, varName, PARAM_NORMAL);
    } else if (IS_PARAM_VAR(varSym)) {
        ICG_OpWithParamVar(mne, varSym);
    } else {
        enum AddrModes addrMode = CALC_SYMBOL_ADDR_MODE(varSym);
        IL_AddInstrP(mne, addrMode, varName, PARAM_NORMAL);

        // Handle 16-bit ops
        if (dataSize > 1) ICG_OpHighByte(mne, varName);
    }

    switch (mne) {
            // these two instructions modify a variable in memory,
            //  so we have to mark it as updated.  If it's already
            //  loaded in a register, then the register needs to be
            //   cleared out.
        case INC:
        case DEC:
            IL_ClearOnUpdate(varSym);
            break;

            // all other instructions will change the accumulator
        default:
            lastUseForAReg = REG_USED_FOR_NOTHING;
            lastStoredAReg = REG_USED_FOR_NOTHING;
            break;
    }
}


void ICG_OpPropertyVar(enum MnemonicCode mne, const SymbolRecord *structSym, const SymbolRecord *propertySym) {
    const char *structName = getVarName(structSym);
    enum AddrModes addrMode = CALC_SYMBOL_ADDR_MODE(structSym);
    unsigned char propertyOfs = (propertySym->location & 0xff);

    IL_AddComment(
            IL_AddInstrS(mne, addrMode, structName, numToStr(propertyOfs), PARAM_NORMAL + PARAM_ADD),
            getStructRefComment("structure ref", structName, propertySym->name));

    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
}

void ICG_OpPropertyVarIndexed(enum MnemonicCode mne, const SymbolRecord *structSym, const SymbolRecord *propertySym) {
    const char *structName = getVarName(structSym);
    const char *propOfsParam = numToStr(propertySym->location & 0xff);

    // DEC and INC operations are special because they don't have Absolute,Y address mode available,
    //   so things need to be done differently
    if ((mne == DEC) || (mne == INC)) {

        // TODO: Maybe find a better way to handle the fact that INC/DEC have limited addressing modes.

        IL_AddComment(
                IL_AddInstrS(LDX, ADDR_ABY, structName, propOfsParam, PARAM_NORMAL + PARAM_ADD),
                getStructRefComment("structure ref", structName, propertySym->name));
        IL_AddInstrB((mne == INC) ? INX : DEX);
        IL_AddInstrB(TXA);
        IL_AddInstrS(STA, ADDR_ABY, structName, propOfsParam, PARAM_NORMAL + PARAM_ADD);
    }
    else {

        IL_AddComment(
                IL_AddInstrS(mne, ADDR_ABY, structName, propOfsParam, PARAM_NORMAL + PARAM_ADD),
                getStructRefComment("structure ref", structName, propertySym->name));
    }

    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
}

void ICG_OpIndexed(enum MnemonicCode mne, const SymbolRecord *varSym, const SymbolRecord *indexSym) {
    const char *varName = getVarName(varSym);
    IL_AddComment(
            IL_AddInstrP(LDX, CALC_SYMBOL_ADDR_MODE(indexSym), getVarName(indexSym), PARAM_NORMAL),
            "loading index var for array op");
    IL_AddComment(
            IL_AddInstrP(mne, CALC_SYMBOL_ADDR_MODE(varSym) + ADDR_X, varName, PARAM_NORMAL),
            "op with data from array using index");

    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
}


void ICG_OpIndexedWithOffset(enum MnemonicCode mne, const SymbolRecord *varSym, int ofs) {
    const char *varName = getVarName(varSym);
    IL_AddComment(
            IL_AddInstrS(mne, ADDR_ABY, varName, numToStr(ofs), PARAM_NORMAL + PARAM_ADD),
            "op with data from array using index with offset");

    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
}

void ICG_OpWithStack(enum MnemonicCode mne) {
    // Do Operation with Accumulator and value from stack
    IL_AddInstrN(TSX, ADDR_NONE, 0);
    IL_AddInstrN(mne, ADDR_ZPX, 1);
    
    // Remove value from stack and fix stack pointer
    IL_AddInstrN(INX, ADDR_NONE, 0);
    IL_AddInstrN(TXS, ADDR_NONE, 0);

    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
}

void ICG_MoveAccToIndex(const char destReg) {
    enum MnemonicCode mne = MNE_NONE;
    switch (destReg) {
        case 'X': mne = TAX; break;
        case 'Y': mne = TAY; break;
    }
    IL_AddInstrN(mne, ADDR_NONE, 0);
    lastUseForYReg = REG_USED_FOR_NOTHING;
}

void ICG_PushAcc() { IL_AddInstrB(PHA); }
void ICG_PullAcc() { IL_AddInstrB(PLA); }
void ICG_Nop() {     IL_AddInstrB(NOP); }

//---------------------------------------------------------

void ICG_IncUsingAddr(const SymbolRecord *baseSymbol, int varOfs, int size) {
    enum AddrModes addrMode = CALC_SYMBOL_ADDR_MODE(baseSymbol);
    const char *varName = getVarName(baseSymbol);
    char *numOfsStr = numToStr(varOfs);

    IL_AddInstrS(INC, addrMode, varName, numOfsStr, PARAM_ADD);
    if (size == 2) {
        IL_AddInstrN(BNE, ADDR_REL, +4);
        IL_AddInstrS(INC, addrMode, varName, numOfsStr, PARAM_ADD + PARAM_PLUS_ONE);
    }
}

void ICG_DecUsingAddr(const SymbolRecord *baseSymbol, int varOfs, int size) {
    enum AddrModes addrMode = CALC_SYMBOL_ADDR_MODE(baseSymbol);
    const char *varName = getVarName(baseSymbol);
    char *numOfsStr = numToStr(varOfs);

    if (size == 2) {
        IL_AddInstrS(LDA, addrMode, varName, numOfsStr, PARAM_ADD);
        IL_AddInstrN(BNE, ADDR_REL, +4);
        IL_AddInstrS(DEC, addrMode, varName, numOfsStr, PARAM_ADD + PARAM_PLUS_ONE);
    }
    IL_AddInstrS(DEC, addrMode, varName, numOfsStr, PARAM_ADD);
}

void ICG_OpWithOffset(enum MnemonicCode mne, SymbolRecord *baseVarSym, int ofs) {
    switch (mne) {
        case INC:
            ICG_IncUsingAddr(baseVarSym, ofs, 1);
            break;
        case DEC:
            ICG_DecUsingAddr(baseVarSym, ofs, 1);
            break;
        default:
            break;
    }
}


void ICG_ShiftLeft(const SymbolRecord *varSym, int count) {
    if (varSym != NULL) ICG_LoadVar(varSym);
    for (int c = 0; c < count; c++) {
        IL_AddInstrN(ASL, ADDR_ACC, 0);
    }
    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
}

void ICG_ShiftRight(const SymbolRecord *varSym, int count) {
    if (varSym != NULL) ICG_LoadVar(varSym);
    for (int c = 0; c < count; c++) {
        IL_AddInstrN(LSR, ADDR_ACC, 0);
    }
    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
}


void ICG_AddToInt(const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);
    IL_AddInstrB(CLC);
    if (IS_PARAM_VAR(varSym)) {
        ICG_OpWithParamVar(ADC, varSym);
    } else {
        IL_AddInstrP(ADC, ADDR_ZP, varName, PARAM_NORMAL);
    }
    IL_AddInstrN(BCC, ADDR_REL, +3);
    IL_AddInstrB(INX);

    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
}

void ICG_AddOffsetToInt(int ofs) {
    IL_AddInstrB(CLC);
    IL_AddInstrN(ADC, ADDR_IMM, ofs);
    IL_AddInstrN(BCC, ADDR_REL, +3);
    IL_AddInstrB(INX);

    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
}

void ICG_AddTempVarToInt(int addr) {
    IL_AddInstrB(CLC);
    IL_AddInstrN(ADC, ADDR_ZP, addr);
    IL_AddInstrN(BCC, ADDR_REL, +3);
    IL_AddInstrB(INX);

    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
}

void ICG_AddAddr(const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);
    IL_AddInstrB(CLC);
    IL_AddInstrP(ADC, ADDR_IMM, varName, PARAM_LO);
    IL_AddInstrP(LDX, ADDR_IMM, varName, PARAM_HI);
    IL_AddInstrN(BCC, ADDR_REL, +3);
    IL_AddInstrB(INX);
    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
}

void ICG_CompareConstName(const char *constName) {
    IL_AddInstrP(CMP, ADDR_IMM, constName, PARAM_NORMAL);
}

void ICG_CompareConst(int constValue) {
    IL_AddInstrN(CMP, ADDR_IMM, constValue);
}

void ICG_CompareVar(const SymbolRecord *varSym) {
    if (isConst(varSym)) {
        IL_AddInstrN(CMP, ADDR_IMM, varSym->constValue);
    } else {
        const char *varName = getVarName(varSym);
        IL_AddInstrP(CMP, ADDR_ZP, varName, PARAM_NORMAL);
    }
}

void ICG_Jump(const Label *label, const char *comment) {
    Label *jumpLabel = (label->link != NULL) ? label->link : (Label *)label;  // use linked label if available
    addLabelRef(jumpLabel);
    IL_AddComment(
            IL_AddInstrP(JMP, ADDR_ABS, jumpLabel->name, PARAM_NORMAL),
            (char *)comment);
}

void ICG_Call(const char *funcName) {
    IL_AddInstrP(JSR, ADDR_ABS, funcName, PARAM_NORMAL);
    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastStoredAReg = REG_USED_FOR_NOTHING;
    lastUseForYReg = REG_USED_FOR_NOTHING;
}

void ICG_Return() {
    IL_AddInstrB(RTS);
}


//----------------------------------------
//  Handle inline assembly

void ICG_AsmData(int value, bool isWord) {
    IL_AddInstrN(isWord ? MNE_DATA_WORD : MNE_DATA, ADDR_NONE, value);
}

