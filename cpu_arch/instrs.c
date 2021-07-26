//
//  Instruction List handling
//
// Created by admin on 4/6/2020.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/common.h"
#include "instrs.h"

static char* curLineComment;

//-------------------------
// register use tracking

typedef struct {
    enum { LW_NONE, LW_CONST, LW_VAR } loadedWith;
    int constValue;
    const SymbolRecord *varSym;
} LastRegisterUse;

const LastRegisterUse REG_USED_FOR_NOTHING = {LW_NONE, 0, NULL};

LastRegisterUse lastUseForAReg;
LastRegisterUse lastUseForXReg;
LastRegisterUse lastUseForYReg;

static int codeSize;
static int codeAddr;

//---------------------------------------------------
//   Instruction List handling

InstrBlock *curBlock;
Label *curLabel;
bool showCycles = false;

Instr* startNewInstruction(enum MnemonicCode mne, enum AddrModes addrMode) {
    Instr *newInstr = malloc(sizeof(struct InstrStruct));
    newInstr->mne = mne;
    newInstr->addrMode = addrMode;
    newInstr->showCycles = showCycles;
    return newInstr;
}

/**
 * Add a new instruction to the instruction list.
 * @param mne - mnemonic of the instruction
 * @param addrMode - address mode
 * @param paramName - parameter name (if needed)
 * @return pointer to new instruction node
 */
Instr* IL_AddInstrS(enum MnemonicCode mne, enum AddrModes addrMode, const char *param1, const char *param2, enum ParamExt paramExt) {
    Instr *newInstr = startNewInstruction(mne, addrMode);
    newInstr->usesVar = true;
    newInstr->paramName = param1;
    newInstr->param2 = param2;
    newInstr->paramExt = paramExt;

    // handle label and comment first
    newInstr->label = curLabel;
    newInstr->lineComment = curLineComment;
    curLabel = NULL;
    curLineComment = NULL;

    IB_AddInstr(curBlock, newInstr);
    return newInstr;
}

Instr* IL_AddInstrN(enum MnemonicCode mne, enum AddrModes addrMode, int ofs) {
    Instr *newInstr = startNewInstruction(mne, addrMode);
    newInstr->usesVar = false;
    newInstr->offset = ofs;
    newInstr->paramName = NULL;
    newInstr->paramExt = PARAM_NORMAL;
    newInstr->param2 = NULL;

    // handle label and comment first
    newInstr->label = curLabel;
    newInstr->lineComment = curLineComment;
    curLabel = NULL;
    curLineComment = NULL;

    IB_AddInstr(curBlock, newInstr);
    return newInstr;
}

Instr* IL_AddInstrB(enum MnemonicCode mne) {
    Instr *newInstr = startNewInstruction(mne, ADDR_NONE);

    // handle label and comment first
    newInstr->label = curLabel;
    newInstr->lineComment = curLineComment;
    curLabel = NULL;
    curLineComment = NULL;

    IB_AddInstr(curBlock, newInstr);
    return newInstr;
}


Instr* IL_AddLabel(Instr *inInstr, Label *label) {
    inInstr->label = label;
    return inInstr;
}

Instr* IL_AddComment(Instr *inInstr, char *comment) {
    inInstr->lineComment = comment;
    return inInstr;
}

void IL_AddCommentToCode(char *comment) {
    IL_AddComment(IL_AddInstrB(MNE_NONE), comment);
}

int IL_GetCodeSize(InstrBlock *instrBlock) {
    Instr *curInstr = instrBlock->firstInstr;
    int size = 0;
    while (curInstr != NULL) {
        int instrSize = getInstrSize(curInstr->mne, curInstr->addrMode);
        //printf("%d\n", instrSize);
        size += instrSize;
        curInstr = curInstr->nextInstr;
    }
    return size;
}

void IL_ShowCycles() {
    showCycles = true;
}

void IL_HideCycles() {
    showCycles = false;
}

//================================================================================

void IL_Init(int startCodeAddr) {
    codeAddr = startCodeAddr;

    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastUseForXReg = REG_USED_FOR_NOTHING;
    lastUseForYReg = REG_USED_FOR_NOTHING;

    curLineComment = NULL;
}

// clear out appropriate register tracker on var update
void IL_ClearOnUpdate(const char *varName) {
    if ((lastUseForAReg.loadedWith == LW_VAR)
        && (strncmp(varName, lastUseForAReg.varSym->name, SYMBOL_NAME_LIMIT) == 0)) {
        lastUseForAReg = REG_USED_FOR_NOTHING;
    }

    if ((lastUseForXReg.loadedWith == LW_VAR)
        && (strncmp(varName, lastUseForXReg.varSym->name, SYMBOL_NAME_LIMIT) == 0)) {
        lastUseForXReg = REG_USED_FOR_NOTHING;
    }

    if ((lastUseForYReg.loadedWith == LW_VAR)
        && (strncmp(varName, lastUseForYReg.varSym->name, SYMBOL_NAME_LIMIT) == 0)) {
        lastUseForYReg = REG_USED_FOR_NOTHING;
    }
}

void IL_Preload(const SymbolRecord *varSym, enum VarHint hint) {
    char *name = (char *)varSym->name;
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
    if (curLabel != NULL && !(label->hasBeenReferenced)) {
        linkToLabel(label, curLabel);
    } else {
        if (label->hasBeenReferenced
               && ((curLabel != NULL) && (curLabel->hasBeenReferenced))) {

            // -- insert a dummy instruction to preserve label until we can consolidate
            IL_AddInstrN(MNE_NONE, ADDR_NONE, 0);
        }
        curLabel = label;
    }

    // labels kill any knowledge of what's in the registers
    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastUseForYReg = REG_USED_FOR_NOTHING;
}

void IL_SetLineComment(const char *comment) {
    curLineComment = (char *)comment;
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

void ICG_LoadFromAddr(int ofs) {
    lastUseForAReg = REG_USED_FOR_NOTHING;
    IL_AddInstrN(LDA, CALC_ADDR_MODE(ofs), ofs);  //ofs < 0x100 ? ADDR_ZP : ADDR_ABS), ofs);
}

void ICG_LoadIntFromAddr(int ofs) {
    lastUseForAReg = REG_USED_FOR_NOTHING;
    enum AddrModes addrMode = CALC_ADDR_MODE(ofs);
    IL_AddInstrN(LDA, addrMode, ofs);
    IL_AddInstrN(LDX, addrMode, ofs+1);
}

void ICG_LoadFromArray(const SymbolRecord *arraySymbol, int index,
                       enum SymbolType destType) {
    // const index, so add to address to read from
    lastUseForAReg = REG_USED_FOR_NOTHING;
    int ofs = arraySymbol->location;
    if (destType == ST_PTR) {
        ofs += (index * 2);
        ICG_LoadIntFromAddr(ofs);
    } else {
        ofs += index;
        ICG_LoadFromAddr(ofs);
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

void ICG_LoadVar(const SymbolRecord *varRec) {
    //if ((lastUseForAReg.loadedWith == LW_VAR) && (lastUseForAReg.varSym == varRec)) return;

    const char *varName = getVarName(varRec);
    if (isConst(varRec)) {
        IL_AddInstrS(LDA, ADDR_IMM, varName, NULL, PARAM_LO);
        if (getBaseVarSize(varRec) == 2) {
            IL_AddInstrS(LDX, ADDR_IMM, varName, NULL, PARAM_HI);
        }
    } else if (IS_PARAM_VAR(varRec)) {
        IL_AddComment(
                IL_AddInstrN(TSX, ADDR_NONE, 0),
                "prepare to read param var");
        IL_AddComment(
                IL_AddInstrN(LDA, ADDR_ABX, 0x100 + (varRec->location)),
                (char *)varName);
    } else {
        IL_AddInstrS(LDA, ADDR_ZP, varName, NULL, PARAM_NORMAL);
        if ((getBaseVarSize(varRec) == 2) || isStructDefined(varRec)) {
            IL_AddInstrS(LDX, ADDR_ZP, varName, "1", PARAM_ADD);
        }
    }

    lastUseForAReg.loadedWith = LW_VAR;
    lastUseForAReg.varSym = varRec;
}

void ICG_LoadIndexVar(const SymbolRecord *varSym, int size) {
    const char *varName = getVarName(varSym);

    bool needToLoad = !((lastUseForYReg.loadedWith == LW_VAR) &&
            (strncmp(getVarName(lastUseForYReg.varSym), varName,SYMBOL_NAME_LIMIT) == 0));

    if (needToLoad) {
        if (size == 2) {
            IL_AddComment(
                    IL_AddInstrS(LDA, ADDR_ZP, varName, NULL, PARAM_NORMAL), "load array index");
            IL_AddInstrN(ASL, ADDR_ACC, 0);
            IL_AddInstrN(TAY, ADDR_NONE, 0);

            lastUseForAReg = REG_USED_FOR_NOTHING;
        } else {
            IL_AddComment(
                    IL_AddInstrS(LDY, ADDR_ZP, varName, NULL, PARAM_NORMAL), "load array index");
        }
        lastUseForYReg.loadedWith = LW_VAR;
        lastUseForYReg.varSym = varSym;
    }
}

void ICG_LoadAddr(const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);
    IL_AddInstrS(LDA, ADDR_IMM, varName, NULL, PARAM_LO);
    IL_AddInstrS(LDX, ADDR_IMM, varName, NULL, PARAM_HI);

    lastUseForAReg.loadedWith = LW_VAR;
    lastUseForAReg.varSym = varSym;
}

void ICG_LoadAddrPlusIndex(const SymbolRecord *varSym, unsigned char index) {
    const char *varName = getVarName(varSym);
    IL_AddInstrS(LDA, ADDR_IMM, varName, numToStr(index), PARAM_ADD + PARAM_LO);
    IL_AddInstrS(LDX, ADDR_IMM, varName, numToStr(index), PARAM_ADD + PARAM_HI);
}

void ICG_LoadIndirect(const SymbolRecord *varSym, int destSize) {
    const char *varName = getVarName(varSym);
    if (destSize == 2) {
        IL_AddComment(
                IL_AddInstrB(INY),
                "load indirect - high byte first");
        IL_AddComment(
                IL_AddInstrS(LDA, ADDR_IY, varName, NULL, PARAM_NORMAL),
                "load from pointer location using index");
        IL_AddInstrB(TAX);
        IL_AddInstrB(DEY);
    }
    IL_AddComment(
            IL_AddInstrS(LDA, ADDR_IY, varName, NULL, PARAM_NORMAL),
            "load from pointer location using index");
}

void ICG_LoadIndexed(const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);
    IL_AddComment(
            IL_AddInstrS(LDA, ADDR_ABY, varName, NULL, PARAM_NORMAL),
            "load from array using index");
    if (getBaseVarSize(varSym) > 1) {
        IL_AddInstrS(LDX, ADDR_ABY, varName, NULL, PARAM_NORMAL + PARAM_PLUS_ONE);
    }
}

void ICG_LoadIndexedWithOffset(const SymbolRecord *varSym, int ofs) {
    const char *varName = getVarName(varSym);
    IL_AddComment(
            IL_AddInstrS(LDA, ADDR_ABY, varName, numToStr(ofs), PARAM_NORMAL + PARAM_ADD),
            "load from array using index with offset");
}

void ICG_LoadRegConst(const char destReg, int ofs) {
    switch (destReg) {
        case 'A': IL_AddInstrN(LDA, ADDR_IMM, ofs); break;
        case 'X': IL_AddInstrN(LDX, ADDR_IMM, ofs); break;
        case 'Y':
            if (!((lastUseForYReg.loadedWith == LW_CONST) && (lastUseForYReg.constValue == ofs))) {
                IL_AddInstrN(LDY, ADDR_IMM, ofs);
                lastUseForYReg.loadedWith = LW_CONST;
                lastUseForYReg.constValue = ofs;
            }
            break;
    }
}

void ICG_LoadFromStack(int ofs) {
    IL_AddInstrN(TSX, ADDR_NONE, 0);
    IL_AddComment(
        IL_AddInstrN(LDA, ADDR_ABX, 0x100 + ofs),
        "load param from stack");
}

void ICG_LoadPointerAddr(const SymbolRecord *varSym) {
    IL_SetLineComment(varSym->name);
    ICG_LoadConst(varSym->location & 0xff, 1);
    ICG_PushAcc();
    ICG_LoadConst(varSym->location >> 8, 1);
    ICG_PushAcc();
}

void ICG_AdjustStack(int ofs) {
    IL_AddInstrN(TSX, ADDR_NONE, 0);
    for (int cnt=0; cnt < ofs; cnt++) {
        IL_AddInstrN(INX, ADDR_NONE, cnt);
    }
    IL_AddInstrN(TXS, ADDR_NONE, 0);
}

//--------------------------------------------------------------

void ICG_StoreToAddr(int ofs, int size) {
    enum AddrModes addrMode = CALC_ADDR_MODE(ofs);
    IL_AddInstrN(STA, addrMode, ofs);
    if (size == 2) {
        IL_AddInstrN(STX, addrMode, ofs + 1);
    }
}

void ICG_StoreVarOffset(const SymbolRecord *varSym, int ofs, int destSize) {
    const char *varName = getVarName(varSym);

    if (isPointer(varSym) && isStructDefined(varSym)) {
        ICG_LoadRegConst('Y', ofs);
        IL_AddInstrS(STA, ADDR_IY, varName, NULL, PARAM_NORMAL);
        if (destSize == 2) {
            IL_AddInstrB(TXA);
            IL_AddInstrB(INY);
            IL_AddInstrS(STA, ADDR_IY, varName, NULL, PARAM_NORMAL);
        }
    } else {
        bool isZP = ((varSym->location + ofs) < 0x100);
        enum AddrModes addrMode = (isZP ? ADDR_ZP : ADDR_ABS);
        enum ParamExt paramExt = (isZP ? PARAM_LO : PARAM_NORMAL);

        if (destSize == 2) {
            IL_AddInstrS(STX, addrMode, varName, numToStr(ofs), paramExt + PARAM_ADD + PARAM_PLUS_ONE);
        }
        IL_AddInstrS(STA, addrMode, varName, numToStr(ofs), paramExt + PARAM_ADD);
    }
}

void ICG_StoreVarIndexed(const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);
    IL_AddInstrS(STA, ADDR_ABY, varName, NULL, PARAM_NORMAL);
    if (getBaseVarSize(varSym) == 2) {
        enum AddrModes addrMode = (varSym->location < 0x100 ? ADDR_ZPY : ADDR_ABY);
        enum ParamExt paramExt = (addrMode == ADDR_ZPY ? PARAM_LO : PARAM_NORMAL);
        IL_AddInstrS(STX, addrMode, varName, "1", PARAM_ADD + paramExt);
    }
}

void ICG_StoreVarSym(const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);
    IL_ClearOnUpdate(varName);
    IL_AddInstrS(STA, ADDR_ZP, varName, NULL, PARAM_NORMAL);
    if (getBaseVarSize(varSym) == 2) {
        IL_AddInstrS(STX, ADDR_ZP, varName, "1", PARAM_ADD);
    }
}

//---------------------------------------------------------

void ICG_Branch(enum MnemonicCode mne, const Label *label) {
    Label *branchLabel = (label->link != NULL) ? label->link : (Label *)label;  // use linked label if available
    addLabelRef(branchLabel);
    IL_AddInstrS(mne, ADDR_REL, branchLabel->name, NULL, PARAM_NORMAL);
}

void ICG_Not() {
    IL_AddInstrS(EOR, ADDR_IMM, "$FF", NULL, PARAM_NORMAL);
}

void ICG_NotBool() {
    IL_AddInstrS(EOR, ADDR_IMM, "$01", NULL, PARAM_NORMAL);
}

void ICG_Negate() {
    IL_AddInstrS(EOR, ADDR_IMM, "$FF", NULL, PARAM_NORMAL);
    IL_AddInstrN(CLC, ADDR_NONE, 0);
    IL_AddInstrN(ADC, ADDR_IMM, 1);
}

void ICG_PreOp(enum MnemonicCode preOp) {
    if (preOp != MNE_NONE) IL_AddInstrN(preOp, ADDR_NONE, 0);
}

void ICG_OpWithConst(enum MnemonicCode mne, int num) {
    IL_AddInstrN(mne, ADDR_IMM, num);
}

void ICG_OpWithVar(enum MnemonicCode mne, const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);
    if (isConst(varSym)) {
        IL_AddInstrS(mne, ADDR_IMM, varName, NULL, PARAM_NORMAL);
    } else if (IS_PARAM_VAR(varSym)) {
        IL_AddComment(
                IL_AddInstrN(TSX, ADDR_NONE, 0),
                "prepare to read param var");
        IL_AddComment(
                IL_AddInstrN(mne, ADDR_ABX, 0x100 + (varSym->location)),
                (char *)varName);
    } else {
        IL_AddInstrS(mne, ADDR_ZP, varName, NULL, PARAM_NORMAL);
    }
    lastUseForAReg = REG_USED_FOR_NOTHING;
    //IL_ClearOnUpdate(varName);
}

void ICG_OpWithAddr(enum MnemonicCode mne, int addr) {
    IL_AddInstrN(mne, CALC_ADDR_MODE(addr), addr);
}

void ICG_OpWithStack(enum MnemonicCode mne) {
    // Do Operation with Accumulator and value from stack
    IL_AddInstrN(TSX, ADDR_NONE, 0);
    IL_AddInstrN(mne, ADDR_ZPX, 1);
    
    // Remove value from stack and fix stack pointer
    IL_AddInstrN(INX, ADDR_NONE, 0);
    IL_AddInstrN(TXS, ADDR_NONE, 0);
}

void ICG_MoveIndexToAcc(const char srcReg) {
    enum MnemonicCode mne = MNE_NONE;
    switch (srcReg) {
        case 'X': mne = TXA; break;
        case 'Y': mne = TYA; break;
    }
    IL_AddInstrN(mne, ADDR_NONE, 0);
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

void ICG_IncUsingAddr(int varOfs, int size) {
    //IL_ClearOnUpdate(varName);
    IL_AddInstrN(INC, ADDR_ZP, varOfs);
    if (size == 2) {
        IL_AddInstrN(BNE, ADDR_REL, +4);
        IL_AddInstrN(INC, ADDR_ZP, varOfs+1);
    }
}

void ICG_DecUsingAddr(int varOfs, int size) {
    if (size == 2) {
        IL_AddInstrN(BNE, ADDR_REL, +4);
        IL_AddInstrN(DEC, ADDR_ZP, varOfs+1);
    }
    IL_AddInstrN(DEC, ADDR_ZP, varOfs);
}

void ICG_ShiftLeft(const SymbolRecord *varSym, int count) {
    if (varSym != NULL) ICG_LoadVar(varSym);
    for (int c = 0; c < count; c++) {
        IL_AddInstrN(ASL, ADDR_ACC, 0);
    }
}

void ICG_ShiftRight(const SymbolRecord *varSym, int count) {
    if (varSym != NULL) ICG_LoadVar(varSym);
    for (int c = 0; c < count; c++) {
        IL_AddInstrN(LSR, ADDR_ACC, 0);
    }
}


void ICG_AddToInt(const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);
    IL_AddInstrN(CLC, ADDR_NONE, 0);
    if (IS_PARAM_VAR(varSym)) {
        IL_AddInstrS(ADC, ADDR_ABS, varName, "$100", PARAM_ADD);
    } else {
        IL_AddInstrS(ADC, ADDR_ZP, varName, NULL, PARAM_NORMAL);
    }
    IL_AddInstrN(BCC, ADDR_REL, +3);
    IL_AddInstrN(INX, ADDR_NONE, 0);
}

void ICG_AddAddr(const SymbolRecord *varSym) {
    const char *varName = getVarName(varSym);
    IL_AddInstrB(CLC);
    IL_AddInstrS(ADC, ADDR_IMM, varName, NULL, PARAM_LO);
    IL_AddInstrS(LDX, ADDR_IMM, varName, NULL, PARAM_HI);
    IL_AddInstrN(BCC, ADDR_REL, +3);
    IL_AddInstrB(INX);
}

void ICG_CompareConstName(const char *constName) {
    IL_AddInstrS(CMP, ADDR_IMM, constName, NULL, PARAM_NORMAL);
}

void ICG_CompareConst(int constValue) {
    IL_AddInstrN(CMP, ADDR_IMM, constValue);
}

void ICG_CompareVar(const SymbolRecord *varSym) {
    if (isConst(varSym)) {
        IL_AddInstrN(CMP, ADDR_IMM, varSym->constValue);
    } else {
        const char *varName = getVarName(varSym);
        IL_AddInstrS(CMP, ADDR_ZP, varName, NULL, PARAM_NORMAL);
    }
}

void ICG_Jump(const Label *label, const char *comment) {
    Label *jumpLabel = (label->link != NULL) ? label->link : (Label *)label;  // use linked label if available
    addLabelRef(jumpLabel);
    IL_AddComment(
            IL_AddInstrS(JMP, ADDR_ABS, jumpLabel->name, NULL, PARAM_NORMAL),
            (char *)comment);
}

void ICG_Call(const char *funcName) {
    IL_AddInstrS(JSR, ADDR_ABS, funcName, NULL, PARAM_NORMAL);
    lastUseForAReg = REG_USED_FOR_NOTHING;
}

void ICG_Return() {
    IL_AddInstrN(RTS, ADDR_NONE, 0);
}


//----------------------------------------
//  Handle inline assembly

void ICG_AsmInstr(enum MnemonicCode mne, enum AddrModes addrMode, const char *paramStr) {
    if (paramStr != NULL) {
        IL_AddInstrS(mne, addrMode, paramStr, NULL, PARAM_NORMAL);
    } else {
        IL_AddInstrN(mne, addrMode, 0);
    }
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


int ICG_StartOfFunction(Label *funcLabel, SymbolRecord *funcSym) {
    curBlock = IB_StartInstructionBlock(funcLabel->name);
    curBlock->funcSym = funcSym;

    IL_Label(funcLabel);
    IB_SetCodeAddr(curBlock, codeAddr);

    // reset register trackers
    lastUseForAReg = REG_USED_FOR_NOTHING;
    lastUseForYReg = REG_USED_FOR_NOTHING;

    // main function needs to run init code
    if (strcmp(curBlock->funcSym->name, compilerOptions->entryPointFuncName) == 0) {
        ICG_SystemInitCode();
    }

    codeSize = 0;
    return codeAddr;
}

InstrBlock* ICG_EndOfFunction() {
    InstrBlock *funcInstrBlock = curBlock;

    // All functions except main() will need a RTS at the end.
    //    Add the RTS if it's not already there
    if (strcmp(curBlock->funcSym->name, compilerOptions->entryPointFuncName) != 0) {
        if ((curBlock->curInstr == NULL) || (curBlock->curInstr->mne != RTS)) ICG_Return();
    }

    // save code size of function (to allow arrangement)
    codeSize = IL_GetCodeSize(curBlock);
    funcInstrBlock->codeSize = codeSize;

    // reset current block
    curBlock = NULL;
    codeAddr += codeSize;

    return funcInstrBlock;
}


//------------------------------------------------------------------------
//   Output static array data

int ICG_StaticArrayData(char *varName, ListNode valueNode) {
    int startAddr = codeAddr;
    codeAddr += valueNode.value.list->count;
    return startAddr;
}

/**
 * Mark section of address space as Static data
 *
 * @param size
 */
int ICG_MarkStaticArrayData(int size) {
    int startAddr = codeAddr;
    codeAddr += size;
    return startAddr;
}