//
//  Math routines
//
//  Handle more complicated math on the 6502.  i.e.  Multiplication
//
//  NOTE:
//      Lookup table code is mainly geared toward table/array of structs lookups.
//      So, it only supports multipliers upto 63.
//
//  TODO:  Cleanup variable handling code.
//          Currently doesn't handle variables in all cases properly (zp vs abs)
//          Also, these routines have some redundant code in them.
//
//  TODO:  Use label generator instead of numeric offsets in branches
//
// Created by admin on 5/11/2021.
//

#include <string.h>
#include <output/output_block.h>
#include "instrs_math.h"
#include "instrs.h"

//------------------------------
//  Internal variables

//-- symbol table currently in scope for looking up symbol records for these routines
SymbolTable *mul_globalSymbolTable;


// For multiply ops, 0x80 + 0x81 are temp zeropage locations used for an accumulator
const int ACC_MUL_ADDR = 0x80;

//---------------------------------------------------------------------------
//--- Table used to generate small quick macro code for multiply operations

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


/**
 * Handle multiplication lookup tables
 */

bool valueLookupTableExists[64];

void ICG_Mul_InitLookupTables(SymbolTable *globalSymbolTable) {
    for_range(i,0,63) {
        valueLookupTableExists[i] = false;
    }
    mul_globalSymbolTable = globalSymbolTable;
}

/**
 * Add a multiplication lookup table
 */
void ICG_Mul_AddLookupTable(char lookupValue) {

    //--- create a list big enough to contain the lookup table
    int count = (256 / lookupValue) + 1;
    List *lookupTableList = createList(count+1);
    addNode(lookupTableList, createParseToken(PT_INIT));

    //------------------------------
    // generate the lookup table

    for_range(index, 0, count) {
        int value = index * lookupValue;
        addNode(lookupTableList, createIntNode(value));
    }
    valueLookupTableExists[lookupValue] = true;

    //--------------------------------------------
    // Now add the symbol and data to the output

    char *lookupTableName = allocMem(8);
    strcpy(lookupTableName, "QL_");
    strcat(lookupTableName, intToStr(lookupValue));
    printf("Adding lookup table named: %s\n", lookupTableName);

    // create symbol, add to OutputBlock, set symbol location
    SymbolRecord *lookupTableRec = addSymbol(mul_globalSymbolTable, lookupTableName, SK_CONST, ST_CHAR, MF_ARRAY);
    OutputBlock *staticData = OB_AddData(lookupTableName, lookupTableRec, lookupTableList, 0);
    setSymbolLocation(lookupTableRec, ICG_MarkStaticArrayData(staticData->blockSize), SS_ROM);
}

bool hasValueLookupTable(char lookupValue) {
    return valueLookupTableExists[lookupValue];
}

void ICG_MultiplyWithConstTable(const SymbolRecord *varRec, const char multiplier) {
    ICG_LoadIndexVar(varRec, 1);

    char lookupTableName[8];
    strcpy(lookupTableName, "QL_");
    strcat(lookupTableName, intToStr(multiplier));

    SymbolRecord *lookupTable = findSymbol(mul_globalSymbolTable, lookupTableName);
    if (lookupTable != NULL) {
        ICG_LoadIndexed(lookupTable);
    }
}


/**
 * Load variable to use in multiplication into Accumulator
 *
 * @param varRec
 */
void ICG_LoadVarForMultiply(const SymbolRecord *varRec) {
    const char *varName = getVarName(varRec);
    enum AddrModes addrMode = CALC_SYMBOL_ADDR_MODE(varRec);
    if (IS_PARAM_VAR(varRec)) {
        IL_AddInstrB(TSX);
        addrMode = ADDR_ABX;
    }
    IL_AddInstrS(LDA, addrMode, varName, "", PARAM_NORMAL);
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
void ICG_GenericMultiplyWithConst(const char multiplier) {
    // need to load variable into temp var
    IL_AddInstrN(STA, ADDR_ZP, ACC_MUL_ADDR);

    //------------------------------------------------------------------------
    //  NOTE: using relative numbers instead of labels for simplification purposes

    IL_AddInstrN(LDA, ADDR_IMM, 0);             // clear out accumulator
    IL_AddInstrN(LDX, ADDR_IMM, 8);             // 8-bits requires 8 loops
    //-- loop start
    IL_AddInstrB(LSR);
    IL_AddInstrN(ROR, ADDR_ZP, ACC_MUL_ADDR);
    IL_AddInstrN(BCC, ADDR_REL, +4);
    IL_AddInstrN(ADC, ADDR_IMM, multiplier-1);      // using -1 to do an add without carry
    //-- skipped over add
    IL_AddInstrB(DEX);
    IL_AddComment(
        IL_AddInstrN(BNE, ADDR_REL, -8),            // Branch back to loop start
        "Branch back to start of multiply loop");

    //-----------------------------------------------------------------------
    IL_AddComment(
        IL_AddInstrB(TAX),                      // move high order byte into X
        "Move high order byte into X");
    IL_AddInstrN(LDA, ADDR_ZP, ACC_MUL_ADDR);
}

/**
 * Generate code for Multiplying two variables together
 *
 * @param varRec
 * @param varRec2
 */
void ICG_MultiplyVarWithVar(const SymbolRecord *varRec, const SymbolRecord *varRec2) {
    // need to load variable into temp var
    ICG_LoadVarForMultiply(varRec);
    IL_AddInstrN(STA, ADDR_ZP, ACC_MUL_ADDR);

    //------------------------------------------------------------------------
    //  NOTE: using relative numbers instead of labels for simplification purposes

    IL_AddInstrN(LDA, ADDR_IMM, 0);             // clear out accumulator
    IL_AddInstrN(LDX, ADDR_IMM, 8);             // 8-bits requires 8 loops
    //-- loop start
    IL_AddInstrB(LSR);
    IL_AddInstrN(ROR, ADDR_ZP, ACC_MUL_ADDR);
    IL_AddInstrN(BCC, ADDR_REL, +4);
    IL_AddInstrB(CLC);
    IL_AddInstrS(ADC, CALC_SYMBOL_ADDR_MODE(varRec2), varRec2->name, "", PARAM_NORMAL);
    //-- skipped over add
    IL_AddInstrB(DEX);
    IL_AddComment(
            IL_AddInstrN(BNE, ADDR_REL, -8),            // Branch back to loop start
            "Branch back to start of multiply loop");

    //-----------------------------------------------------------------------
    IL_AddComment(
            IL_AddInstrB(TAX),                      // move high order byte into X
            "Move high order byte into X");
    IL_AddInstrN(LDA, ADDR_ZP, ACC_MUL_ADDR);
}

/**
 * Generate code for Multiplying an expression with a variable
 *
 * Assumes expression has been evaluated and is loaded into tempOfs
 *
 * @param varLoc1
 * @param varRec2
 */
void ICG_MultiplyExprWithVar(const int varLoc1, const SymbolRecord *varRec2) {
    IL_AddInstrN(STA, ADDR_ZP, ACC_MUL_ADDR);

    //------------------------------------------------------------------------
    //  NOTE: using relative numbers instead of labels for simplification purposes

    IL_AddInstrN(LDA, ADDR_IMM, 0);             // clear out accumulator
    IL_AddInstrN(LDX, ADDR_IMM, 8);             // 8-bits requires 8 loops
    //-- loop start
    IL_AddInstrB(LSR);
    IL_AddInstrN(ROR, ADDR_ZP, ACC_MUL_ADDR);
    IL_AddInstrN(BCC, ADDR_REL, +4);
    IL_AddInstrB(CLC);
    IL_AddInstrS(ADC, CALC_SYMBOL_ADDR_MODE(varRec2), varRec2->name, "", PARAM_NORMAL);
    //-- skipped over add
    IL_AddInstrB(DEX);
    IL_AddComment(
            IL_AddInstrN(BNE, ADDR_REL, -8),            // Branch back to loop start
            "Branch back to start of multiply loop");

    //-----------------------------------------------------------------------
    IL_AddComment(
            IL_AddInstrB(TAX),                      // move high order byte into X
            "Move high order byte into X");
    IL_AddInstrN(LDA, ADDR_ZP, ACC_MUL_ADDR);
}

/**
 * Use a bunch of instruction steps to perform Multiplication
 * @param varRec
 * @param multiplier
 */
void ICG_StepMultiplyWithConst(const SymbolRecord *varRec, const char multiplier) {
    if (multiplier > 16) return;

    const char *varName = getVarName(varRec);
    enum AddrModes addrMode = IS_PARAM_VAR(varRec) ? ADDR_ABX : CALC_SYMBOL_ADDR_MODE(varRec);


    IL_AddInstrN(CLC, ADDR_NONE, 0);
    for (int step=0; step<5; step++) {
        enum MnemonicCode instrMne = multiplierSteps[multiplier-1][step];
        switch (instrMne) {
            case ADC:
            case SBC:
                IL_AddInstrB(instrMne == SBC ? SEC : CLC);
                IL_AddInstrS(instrMne, addrMode, varName, "", PARAM_NORMAL);
                break;
            case ASL:
                IL_AddInstrB(ASL);
                break;
            default:
                break;
        }
    }
    //IL_AddInstrS(STA, addrMode, varName, "", PARAM_NORMAL);
}



void ICG_MultiplyVarWithConst(const SymbolRecord *varRec, const char multiplier) {
    IL_AddCommentToCode("Start of Multiplication");

    if (hasValueLookupTable(multiplier)) {
        ICG_MultiplyWithConstTable(varRec, multiplier);
    } else if (multiplier < 17) {
        ICG_LoadVarForMultiply(varRec);
        ICG_StepMultiplyWithConst(varRec, multiplier);
    } else {
        // do multiply using a generic routine
        ICG_LoadVarForMultiply(varRec);
        ICG_GenericMultiplyWithConst(multiplier);
    }

    IL_AddCommentToCode("End of Multiplication");
}

/**
 * Multiply currently loaded accumulator with const value
 *
 * @param multiplier
 */
void ICG_MultiplyWithConst(const char multiplier) {
    IL_AddCommentToCode("Start of Multiplication Acc with Const");
    ICG_GenericMultiplyWithConst(multiplier);
    IL_AddCommentToCode("End of Multiplication");
}