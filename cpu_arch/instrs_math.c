//
// Created by admin on 5/11/2021.
//

#include "instrs_math.h"
#include "instrs.h"

typedef enum MnemonicCode MultiplierSteps[5];

const MultiplierSteps multiplierSteps[16] = {
    {NOP, NOP, NOP, NOP, NOP}, //1  - limit 255
    {ASL, NOP, NOP, NOP, NOP}, //2  - limit 128
    {ASL, ADC, NOP, NOP, NOP}, //3  - limit 85
    {ASL, ASL, NOP, NOP, NOP}, //4  - limit 64
    {ASL, ASL, ADC, NOP, NOP}, //5  - limit 51
    {ASL, ADC, ASL, NOP, NOP}, //6  - limit 42
    {ASL, ASL, ASL, SBC, NOP}, //7  - limit 36-
    {ASL, ASL, ASL, NOP, NOP}, //8  - limit 32
    {ASL, ASL, ASL, ADC, NOP}, //9  - limit 28
    {ASL, ASL, ADC, ASL, NOP}, //10 - limit 25
    {ASL, ASL, ADC, ASL, ADC}, //11 - limit 23
    {ASL, ADC, ASL, ASL, NOP}, //12 - limit 21
    {ASL, ADC, ASL, ASL, ADC}, //13 - limit 19
    {ASL, ASL, ASL, SBC, ASL}, //14 - limit 18-
    {ASL, ASL, ASL, ASL, SBC}, //15 - limit 17-
    {ASL, ASL, ASL, ASL, NOP}, //16 - limit 16
};

void ICG_Mul_loadVariable(const SymbolRecord *varRec, int addrMulVar, bool isParamVar) {
    const char *varName = getVarName(varRec);
    if (isParamVar) {
        IL_AddInstrB(TSX);
        IL_AddInstrS(LDA, ADDR_ABX, varName, "$100", PARAM_ADD);
    } else {
        IL_AddInstrS(LDA, CALC_ADDR_MODE(addrMulVar), varName, "", PARAM_NORMAL);
    }
}

/*
 * https://atariage.com/forums/topic/71120-6502-killer-hacks/?do=findComment&comment=896028
 *
 * Computing 8x8->16 multiply is more useful than 8x8->8, and isn't really any harder.
 * The 6502's lack of add-without-carry is somewhat irksome, but there are a variety
 * of workarounds that could be used.

; Compute mul1*mul2+acc -> acc:mul1 [mul2 is unchanged]

 ldx #8
 dec mul2
lp:
 lsr
 ror mul1
 bcc nope
 adc mul2  ;-- OR,  #[multiplier-1]
nope:
 dex
 bne lp
 inc mul2

 */
void ICG_GenericMultiplyWithConst(const SymbolRecord *varRec, const char multiplier) {
    int addrMulVar = varRec->location;
    bool isParamVar =  (IS_PARAM_VAR(varRec));

    printSingleSymbol(stdout, varRec);

    int tempOfs = 0x80;

    // need to load variable into temp var
    ICG_Mul_loadVariable(varRec, addrMulVar, isParamVar);
    IL_AddInstrN(STA, ADDR_ZP, tempOfs);

    //------------------------------------------------------------------------
    //  NOTE: using relative numbers instead of labels for simplification purposes

    IL_AddInstrN(LDA, ADDR_IMM, 0);             // clear out accumulator
    IL_AddInstrN(LDX, ADDR_IMM, 8);             // 8-bits requires 8 loops
    //-- loop start
    IL_AddInstrB(LSR);
    IL_AddInstrN(ROR, ADDR_ZP, tempOfs);
    IL_AddInstrN(BCC, ADDR_REL, +4);
    IL_AddInstrN(ADC, ADDR_IMM, multiplier-1);      // using -1 to do an add without carry
    //-- skipped over add
    IL_AddInstrB(DEX);
    IL_AddComment(
        IL_AddInstrN(BNE, ADDR_REL, -8),            // Branch back to loop start
        "Branch back to start of multiply loop");

    //-----------------------------------------------------------------------
    IL_AddInstrB(TAX);                      // move high order byte into X
    IL_AddInstrN(LDA, ADDR_ZP, tempOfs);
}

/**
 * Use a bunch of instruction steps to perform Multiplication
 * @param varRec
 * @param multiplier
 */
void ICG_StepMultiplyWithConst(const SymbolRecord *varRec, const char multiplier) {
    if (multiplier > 16) return;

    bool isParamVar =  (IS_PARAM_VAR(varRec));
    int addrMulVar = varRec->location;
    const char *varName = getVarName(varRec);

    ICG_Mul_loadVariable(varRec, addrMulVar, isParamVar);
    IL_AddInstrN(CLC, ADDR_NONE, 0);
    for (int step=0; step<5; step++) {
        enum MnemonicCode instrMne = multiplierSteps[multiplier-1][step];
        switch (instrMne) {
            case ADC:
            case SBC:
                IL_AddInstrB(instrMne == SBC ? SEC : CLC);
                if (isParamVar) {
                    IL_AddInstrS(instrMne, ADDR_ABX, varName, "$100", PARAM_ADD);
                } else {
                    IL_AddInstrS(instrMne, CALC_ADDR_MODE(addrMulVar), varName, "", PARAM_NORMAL);
                }
                break;
            case ASL:
                IL_AddInstrB(ASL);
                break;
            default:
                break;
        }
    }
    if (isParamVar) {
        IL_AddInstrS(STA, ADDR_ABX, varName, "$100", PARAM_ADD);
    } else {
        IL_AddInstrS(STA, CALC_ADDR_MODE(addrMulVar), varName, "", PARAM_NORMAL);
    }
}



void ICG_MultiplyWithConst(const SymbolRecord *varRec, const char multiplier) {
    IL_AddCommentToCode("Start of Multiplication");

    if (multiplier < 17) {
        ICG_StepMultiplyWithConst(varRec, multiplier);
    } else {
        // do multiply using a generic routine
        ICG_GenericMultiplyWithConst(varRec, multiplier);
    }

    IL_AddCommentToCode("End of Multiplication");
}