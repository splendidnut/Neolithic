//
// Created by admin on 4/6/2020.
//

#ifndef MODULE_INSTRS_H
#define MODULE_INSTRS_H

#include "data/instr_list.h"
#include "data/syntax_tree.h"

// Macro to consolidate all cases where address mode is calculated from location
#define CALC_SYMBOL_ADDR_MODE(symbol) ((((symbol)->flags & SS_STORAGE_MASK) == SS_ZEROPAGE) ? ADDR_ZP : ADDR_ABS)

//----------------------------------------------
//  Register-Use Tracking

enum LastRegisterUseType { LW_NONE, LW_CONST, LW_VAR };

typedef struct {
    enum LastRegisterUseType loadedWith;
    int constValue;
    const SymbolRecord *varSym;
} LastRegisterUse;

extern void ICG_Tag(char regName, SymbolRecord *varSym);
extern bool ICG_IsCurrentTag(char regName, SymbolRecord *varSym);



//----------------------------------------------------------------
//  Here are the "actual" intermediate operations

extern void ICG_LoadFromArray(const SymbolRecord *arraySymbol, int index, enum SymbolType destType);
extern void ICG_LoadConst(int constValue, int size);

extern void ICG_LoadVar(const SymbolRecord *varRec);
extern void ICG_LoadIndexVar(const SymbolRecord *varSym, int size);
extern void ICG_LoadAddr(const SymbolRecord *varSym);
extern void ICG_LoadAddrPlusIndex(const SymbolRecord *varSym, unsigned char index);
extern void ICG_LoadIndirect(const SymbolRecord *varSym, int destSize);
extern void ICG_LoadIndexed(const SymbolRecord *varSym);
extern void ICG_LoadIndexedWithOffset(const SymbolRecord *varSym, int ofs);
extern void ICG_LoadPropertyVar(const SymbolRecord *structSym, const SymbolRecord *propertySym);
extern void ICG_LoadRegConst(const char destReg, int ofs);
extern void ICG_LoadRegVar(const SymbolRecord *varSym, char destReg);
extern void ICG_LoadPointerAddr(const SymbolRecord *varSym);

extern void ICG_AdjustStack(int ofs);

extern void ICG_StoreToAddr(int ofs, int size);
extern void ICG_StoreVarOffset(const SymbolRecord *varSym, int ofs, int destSize);
extern void ICG_StoreVarIndexed(const SymbolRecord *varSym);
extern void ICG_StoreVarSym(const SymbolRecord *varSym);
extern void ICG_StoreIndexedWithOffset(const SymbolRecord *varSym, int ofs);

extern void ICG_Branch(enum MnemonicCode mne, const Label *label);

extern void ICG_Not();
extern void ICG_NotBool();
extern void ICG_Negate();
extern void ICG_PreOp(enum MnemonicCode preOp);

extern void ICG_OpWithConst(enum MnemonicCode mne, int num, int dataSize);
extern void ICG_OpWithVar(enum MnemonicCode mne, const SymbolRecord *varSym, int dataSize);
extern void ICG_OpPropertyVar(enum MnemonicCode mne, const SymbolRecord *structSym, const SymbolRecord *propertySym);
extern void ICG_OpPropertyVarIndexed(enum MnemonicCode mne, const SymbolRecord *structSym, const SymbolRecord *propertySym);
extern void ICG_OpIndexed(enum MnemonicCode mne, const SymbolRecord *varSym);
extern void ICG_OpIndexedWithOffset(enum MnemonicCode mne, const SymbolRecord *varSym, int ofs);
extern void ICG_OpWithStack(enum MnemonicCode mne);

extern void ICG_MoveAccToIndex(const char destReg);
extern void ICG_PushAcc();
extern void ICG_PullAcc();

extern void ICG_OpWithOffset(enum MnemonicCode mne, SymbolRecord *baseVarSym, int ofs);

extern void ICG_ShiftLeft(const SymbolRecord *varSym, int count);
extern void ICG_ShiftRight(const SymbolRecord *varSym, int count);
extern void ICG_AddToInt(const SymbolRecord *varSym);
extern void ICG_AddAddr(const SymbolRecord *varSym);
extern void ICG_CompareConstName(const char *constName);
extern void ICG_CompareConst(int constValue);
extern void ICG_CompareVar(const SymbolRecord *varSym);

extern void ICG_Jump(const Label *label, const char* comment);
extern void ICG_Call(const char *funcName);
extern void ICG_Return();

//-----------------------------------------------------------------------
//---- Handle other things:  inline assembly, functions, static data

extern void ICG_AsmData(int value);

// function specific stuff
extern int ICG_StartOfFunction(Label *funcLabel, SymbolRecord *funcSym);
extern InstrBlock* ICG_EndOfFunction();

// general purpose stuff
extern int ICG_MarkStaticArrayData(int size);

#endif //MODULE_INSTRS_H
