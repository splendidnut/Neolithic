
//-----------------------------------------------------------------------
//  Assembler code block processor
//-----------------------------------------------------------------------
//
// Created by User on 8/19/2021.
//

#include <string.h>
#include <stdlib.h>

#include "gen_asmcode.h"
#include "gen_common.h"
#include "eval_expr.h"
#include "cpu_arch/instrs.h"


static enum AddrModes paramAddrMode;       // indicate whether an ASM param uses ZP or ABS modes
static char* param2;
enum ParamExt paramExt;

void GC_Asm_HandlePropertyRef(List *propertyRef, char *paramStr) {
    if ((propertyRef->nodes[1].type != N_STR)
        || (propertyRef->nodes[2].type != N_STR)) {
        ErrorMessageWithList("GC_Asm_HandlePropertyRef: Invalid property reference", propertyRef);
        return;
    }
    SymbolRecord *structSym = lookupSymbolNode(propertyRef->nodes[1], propertyRef->lineNum);
    SymbolRecord *propertySym = findSymbol(getStructSymbolSet(structSym), propertyRef->nodes[2].value.str);

    if (!(structSym && propertySym)) {
        ErrorMessageWithList("GC_Asm_HandlePropertyRef: Invalid property reference", propertyRef);
        return;
    }

    paramAddrMode = (structSym->location < 256) ? ADDR_ZP : ADDR_ABS;

    sprintf(paramStr, "%s", getVarName(structSym));
    param2 = intToStr(propertySym->location);
    paramExt = PARAM_ADD;
}

void GC_Asm_HandleArrayLookup(List *arrayLookup, char *paramStr) {
    if ((arrayLookup->nodes[1].type != N_STR)
        ||(arrayLookup->nodes[2].type != N_INT)) {
        ErrorMessageWithList("Invalid array lookup", arrayLookup);
        return;
    }
    SymbolRecord *arraySym = lookupSymbolNode(arrayLookup->nodes[1], arrayLookup->lineNum);
    if (!arraySym) {
        ErrorMessageWithList("Couldn't find array symbol", arrayLookup);
        return;
    }

    int baseSize = getBaseVarSize(arraySym);

    paramAddrMode = (arraySym->location < 256) ? ADDR_ZP : ADDR_ABS;

    sprintf(paramStr, "%s", getVarName(arraySym));
    param2 = intToStr(arrayLookup->nodes[2].value.num * baseSize);
    paramExt = PARAM_ADD;
}

void GC_Asm_EvalExpression(const List *paramExpr, char *paramStr) {
    setEvalExpressionMode(true);
    EvalResult evalResult = evaluate_expression(paramExpr);
    if (evalResult.hasResult) {
        int ofs = evalResult.value;
        IL_SetLineComment(get_expression(paramExpr));
        strcpy(paramStr, intToStr(ofs));
        paramAddrMode = (ofs < 256) ? ADDR_ZP : ADDR_ABS;
    } else {
        // evaluate the expression and use the string result
        strcpy(paramStr, get_expression(paramExpr));
        paramAddrMode = ADDR_ABS;
    }
    setEvalExpressionMode(false);
}

void GC_Asm_ParamExpr(List *paramExpr, char *paramStr) {
    switch (paramExpr->nodes[0].value.parseToken) {
        case PT_PROPERTY_REF:
            GC_Asm_HandlePropertyRef(paramExpr, paramStr);
            return;

        case PT_LOOKUP:
            GC_Asm_HandleArrayLookup(paramExpr, paramStr);
            return;

        default: {
            GC_Asm_EvalExpression(paramExpr, paramStr);
            return;
        }
    }
}

char *GC_Asm_getParamStr(ListNode instrParamNode, List *instr) {
    char *paramStr;
    switch (instrParamNode.type) {
        case N_LIST:
            paramStr = malloc(SYMBOL_NAME_LIMIT);
            paramStr[0] = '\0';
            GC_Asm_ParamExpr(instrParamNode.value.list, paramStr);
            break;
        case N_INT:
            paramStr = intToStr(instrParamNode.value.num);
            //sprintf(paramStr, "%d", instrParamNode.value.num);
            break;
        case N_STR: {
            // first, attempt to find label... if that fails, attempt to find the symbol
            Label *asmLabel = findLabel(instrParamNode.value.str);
            if (asmLabel) {
                paramStr = strdup(asmLabel->name);
            } else {
                SymbolRecord *varSym = lookupSymbolNode(instrParamNode, instr->lineNum);
                if (varSym != NULL) {
                    paramAddrMode = (varSym->location < 256) ? ADDR_ZP : ADDR_ABS;
                    paramStr = strdup(getVarName(varSym));
                } else {
                    paramStr = "";
                }
            }
        } break;
    }
    return paramStr;
}

/**
 * Process an ASM instruction that is in an assembly block
 *
 *  - Process parameter string
 *  - Fix address mode issues
 *  - Validate opcode
 *  - Store in Instruction List
 *
 * @param instr
 */
void GC_AsmInstr(List *instr) {
    enum AddrModes addrMode = ADDR_NONE;
    char *paramStr = NULL;

    // need to reset these before every instruction
    param2 = NULL;
    paramExt = PARAM_NORMAL;

    enum MnemonicCode mne = instr->nodes[0].value.mne;

    if (instr->count > 1) {
        addrMode = instr->nodes[1].value.addrMode;
        if (addrMode > 1) {
            paramStr = GC_Asm_getParamStr(instr->nodes[2], instr);
        }
    }

    // need to patch over incorrect address modes with the correct ones
    //    because the parser just takes a premature guess
    if ((mne == JMP) && (addrMode != ADDR_IND)) addrMode = ADDR_ABS;
    if (mne == JSR) addrMode = ADDR_ABS;

    // Handle cases where symbol parameter affects which addressing mode we use
    //   Will fail towards using absolute addressing modes.
    if (addrMode >= ADDR_INCOMPLETE) {
        switch (addrMode) {
            case ADDR_UNK_M:
                addrMode = (paramAddrMode == ADDR_ZP) ? ADDR_ZP : ADDR_ABS;
                break;
            case ADDR_UNK_MX:
                addrMode = (paramAddrMode == ADDR_ZP) ? ADDR_ZPX : ADDR_ABX;
                break;
            case ADDR_UNK_MY:
                addrMode = (paramAddrMode == ADDR_ZP) ? ADDR_ZPY : ADDR_ABY;
                break;
        }
    }

    // need to fix issue with non-existent ZPY modes (switch to ABY)
    OpcodeEntry opcodeEntry = lookupOpcodeEntry(mne, addrMode);
    if ((opcodeEntry.mneCode == MNE_NONE) && (addrMode == ADDR_ZPY)) {
        addrMode = ADDR_ABY;
    }


    if (paramStr != NULL) {
        IL_AddInstrS(mne, addrMode, paramStr, param2, paramExt);
    } else {
        IL_AddInstrN(mne, addrMode, 0);
    }

    // NOTE: -- CANNOT free paramStr as it needs to stick around for the instruction list
}

void GC_NewConst(char *equName, int value) {
    SymbolRecord *equSymbol = addSymbol(curFuncSymbolTable, equName, SK_CONST, ST_CHAR, 0);
    equSymbol->isLocal = true;
    equSymbol->constValue = value;
    equSymbol->hasValue = true;
}

void GC_AsmBlock(const List *code, enum SymbolType destType) {
    // TODO: Collect all local labels into a "local" label list (instead of global label list)

    //  First, collect all the labels  (allows labels to be defined after usage)

    for_range (stmtNum, 1, code->count) {
        ListNode stmtNode = code->nodes[stmtNum];
        if (stmtNode.type == N_LIST) {
            List *statement = stmtNode.value.list;
            if (isToken(statement->nodes[0], PT_LABEL)) {
                newLabel(statement->nodes[1].value.str, L_CODE);
            }
        }
    }

    //-------------------------------------------------------------
    //  Now process all the code

    for_range (stmtNum, 1, code->count) {
        ListNode stmtNode = code->nodes[stmtNum];

        if (stmtNode.type == N_LIST) {
            List *statement = stmtNode.value.list;
            ListNode instrNode = statement->nodes[0];

            if (instrNode.type == N_STR || instrNode.type == N_MNE) {
                // handle ASM instruction
                GC_AsmInstr(statement);

            } else if (instrNode.value.parseToken == PT_EQUATE) {
                // handle const def
                char *equName = statement->nodes[1].value.str;
                int value = statement->nodes[2].value.num;
                //WO_Variable(equName, value);

                // Add as constant to local function scope
                // TODO: Check this feature
                GC_NewConst(equName, value);
            } else if (instrNode.value.parseToken == PT_LABEL) {
                // handle label def
                char *labelName = statement->nodes[1].value.str;
                Label *asmLabel = findLabel(labelName);
                IL_Label(asmLabel);
            } else if (instrNode.value.parseToken == PT_INIT) {
                ICG_AsmData(statement->nodes[1].value.num);
            }
        }
    }
}
