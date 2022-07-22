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
//  Generate Code by walking the abstract syntax tree
//
// Created by admin on 3/28/2020.
//
//

#include <stdio.h>

#include "gen_code.h"
#include "common/common.h"
#include "data/symbols.h"
#include "cpu_arch/instrs.h"
#include "cpu_arch/instrs_math.h"
#include "eval_expr.h"
#include "output/output_block.h"
#include "common/tree_walker.h"
#include "gen_common.h"
#include "parser/parse_directives.h"
#include "flatten_tree.h"
#include "gen_asmcode.h"
#include "type_checker.h"

//-------------------------------------------
//  Variables used in code generation

static bool reverseData;
enum CompilerDirectiveTokens lastDirective;
int curBank = 0;

//--------------------------------------------------------
//--- Forward References

void GC_Expression(const List *expr, enum SymbolType destType);
void GC_Statement(List *stmt);
void GC_CodeBlock(List *code);
void GC_FuncCall(const List *stmt, enum SymbolType destType);
void GC_FuncCallExpression(const List *stmt, enum SymbolType destType);

//-----------------------------------------------------------------------------
//  Some utility functions

SymbolRecord* getArraySymbol(const List *expr, ListNode arrayNode) {
    SymbolRecord *arraySymbol = lookupSymbolNode(arrayNode, expr->lineNum);

    if ((arraySymbol != NULL) && (!isArray(arraySymbol) && !isPointer(arraySymbol))) {
        ErrorMessageWithNode("Not an array or pointer", arrayNode, expr->lineNum);
        arraySymbol = NULL;
    }
    return arraySymbol;
}

//-------------------------------------------------------
//   Simple Code Generation


void GC_LoadVar(ListNode loadNode, int lineNum) {
    SymbolRecord *varSymbol = lookupSymbolNode(loadNode, lineNum);
    if (varSymbol != NULL) ICG_LoadVar(varSymbol);
}

void GC_LoadPrimitive(ListNode loadNode, enum SymbolType destType, int lineNum) {
    int destSize = (destType == ST_INT) ? 2 : 1;
    if (loadNode.type == N_STR) {
        GC_LoadVar(loadNode, lineNum);
    } else if (loadNode.type == N_INT) {
        ICG_LoadConst(loadNode.value.num, destSize);
    } else {
        ErrorMessageWithNode("Error loading primitive", loadNode, lineNum);
    }
}

void GC_HandleLoad(ListNode loadNode, enum SymbolType destType, int lineNum) {
    /*
    // Handle the special case where a 16-bit number is being set from an 8-bit value
    bool is16bitDest = ((destType == ST_INT) || (destType == ST_PTR));
    if (is16bitDest && ((loadNode.type == N_STR) || (loadNode.type == N_INT))) {
        ICG_LoadRegConst('X',0);
    }
    */
    if (loadNode.type == N_LIST) {
        GC_Expression(loadNode.value.list, destType);
    } else {
        GC_LoadPrimitive(loadNode, destType, lineNum);
    }
}

void GC_StoreInVar(const ListNode symbolNode, enum SymbolType destType, int lineNum) {
    SymbolRecord *varSymRec = lookupSymbolNode(symbolNode, lineNum);
    if (varSymRec != NULL) {
        ICG_StoreVarSym(varSymRec);
    }
}

//---------------------------------------------------------------------
//--- Handle Arrays
//---------------------------------------------------------------------

void GC_LoadFromArray(const SymbolRecord *srcSym, enum SymbolType destType) {
    if ((isPointer(srcSym) && !isArray(srcSym))) {
        int destSize = (destType == ST_INT) ? 2 : 1;
        ICG_LoadIndirect(srcSym, destSize);
    } else {
        ICG_LoadIndexed(srcSym);
    }
}

void GC_ArrayLookupWithExpr(const List *expr, const SymbolRecord *arraySymbol, ListNode indexNode, int destSize) {
    List *indexExpr = indexNode.value.list;

    // detect if simple expression:  (varName+int)
    bool isAddSub = isToken(indexExpr->nodes[0], PT_ADD) || isToken(indexExpr->nodes[0], PT_SUB);
    bool isSecondParamInt = (indexExpr->nodes[2].type == N_INT);

    if (isAddSub && isSecondParamInt) {
        int ofs = indexExpr->nodes[2].value.num;
        SymbolRecord *arrayIndexSymbol = lookupSymbolNode(indexExpr->nodes[1], expr->lineNum);
        if (arrayIndexSymbol != NULL) {
            ICG_LoadIndexVar(arrayIndexSymbol, destSize);
            ICG_LoadIndexedWithOffset(arraySymbol, ofs);
        }
    } else {
        // handle normal (potentially complex) expression

        GC_Expression(indexExpr, ST_NONE);
        ICG_MoveAccToIndex('Y');
        ICG_LoadIndexed(arraySymbol);
    }
}

/**
 * Array Lookup operation (done within an expression)
 * @param expr
 * @param destType
 */
void GC_HandleArrayLookup(const List *expr, enum SymbolType destType) {
    SymbolRecord *arraySymbol = getArraySymbol(expr, expr->nodes[1]);
    ListNode indexNode = expr->nodes[2];

    if (arraySymbol == NULL) return;

    int destSize = (destType == ST_INT || destType == ST_PTR) ? 2 : 1;

    switch (indexNode.type) {
        case N_STR: {
            /*** Array indexed via Variable:  arrayName[varName] ***/
            SymbolRecord *arrayIndexSymbol = lookupSymbolNode(indexNode, expr->lineNum);
            if (arrayIndexSymbol != NULL) {
                ICG_LoadIndexVar(arrayIndexSymbol, destSize);
                GC_LoadFromArray(arraySymbol, destType);
            }
        } break;
        case N_INT:
            /*** Array indexed with number:  arrayName[num] ***/
            ICG_LoadFromArray(arraySymbol, indexNode.value.num, destType);
            break;
        case N_LIST:
            /*** Array indexed with expression:  arrayName[expr] ***/
            GC_ArrayLookupWithExpr(expr, arraySymbol, indexNode, destSize);
            break;
    }
}

void GC_HandleAddrOfArray(const List *expr, enum SymbolType destType) {
    ListNode arrayNameNode = expr->nodes[1].value.list->nodes[1];
    SymbolRecord *arraySymbol = getArraySymbol(expr, arrayNameNode);
    ListNode indexNode = expr->nodes[2];

    if (arraySymbol == NULL) return;

    switch (indexNode.type) {
        case N_STR: {
            /*** Array indexed via Variable:  &arrayName[varName] ***/
            SymbolRecord *arrayIndexSymbol = lookupSymbolNode(indexNode, expr->lineNum);
            if (arrayIndexSymbol != NULL) {
                ICG_LoadAddr(arraySymbol);
                ICG_AddToInt(arrayIndexSymbol);     // TODO: need to check if datatype size is needed
            }
        } break;
        case N_INT:
            /*** Array indexed with number:  &arrayName[num] ***/
            if (!isPointer(arraySymbol)) {
                int arrayElementSize = getBaseVarSize(arraySymbol);
                int arrayIndex = indexNode.value.num;
                ICG_LoadAddrPlusIndex(arraySymbol, arrayIndex * arrayElementSize);
            }
            break;
        case N_LIST:
            /*** Array indexed with expression:  &arrayName[expr] ***/
            //printf("GC_HandleAddrOfArray: &%s[expr]\n", arrayNameNode.value.str);
            GC_Expression(indexNode.value.list, ST_CHAR);
            ICG_AddAddr(arraySymbol);
            break;
        default:
            ErrorMessageWithList("Error in AddrOf array lookup: ", expr);
            break;
    }
}

void GC_Lookup(const List *expr, enum SymbolType destType) {
    if (expr->nodes[1].type == N_LIST) {
        /*** Need to handle addressOf operator:   &array[--] ***/
        GC_HandleAddrOfArray(expr, destType);
    } else {
        GC_HandleArrayLookup(expr, destType);
    }
}

//------------------------------------------------------------
//  Helper functions for array lookups

//  Three cases of array lookups:
//    (lookup, arrayName, indexNum)   -- index is number or const, so can be applied directly to op param
//    (lookup, arrayName, indexVar)   -- index is in a variable... so it must be loaded into Y reg
//    (lookup, arrayName, expr)       -- index is calculated... so it must be processed and loaded into Y reg

bool isArrayIndexConst(const List *arrayExpr) {
    ListNode indexNode = arrayExpr->nodes[2];
    if (indexNode.type == N_INT) return true;
    if (indexNode.type != N_STR) return false;

    // At this point, we definitely have a symbol, so check and see if it's a CONST kind of symbol
    SymbolRecord *indexSym = lookupSymbolNode(indexNode, arrayExpr->lineNum);
    return (indexSym && isConst(indexSym));
}

int GC_GetArrayIndex(const SymbolRecord* arraySymbol, const List *expr) {
    if (arraySymbol == NULL) return -1;

    SymbolRecord* varSym;
    ListNode indexNode = expr->nodes[2];
    int multiplier = getBaseVarSize(arraySymbol);
    switch (indexNode.type) {
        case N_INT:
            return (indexNode.value.num * multiplier);
        case N_STR:
            varSym = lookupSymbolNode(indexNode, expr->lineNum);
            if (varSym && isConst(varSym)) {
                return (varSym->constValue * multiplier);
            }
            break;
        default:
            ErrorMessageWithList("Invalid array lookup", expr);
    }
    return -1;
}


//-------------------------------------------------------------------------------------------
//  Functions to handle Struct property references
//---


// TODO: Improve this function to be able to handle more cases.
//       Need to handle referencing struct pointer case.
//       Need to produce better ASM source code.

enum ParseToken GC_LoadAlias(const List *expr, SymbolRecord *structSymbol) {
    List *alias = structSymbol->alias;

    // exit early if alias is already loaded
    if (ICG_IsCurrentTag('Y', structSymbol)) return PT_LOOKUP;

    // get alias type
    enum ParseToken aliasType;
    if (alias->nodes[0].type == N_TOKEN) {
        aliasType = alias->nodes[0].value.parseToken;
    } else {
        aliasType = PT_EMPTY;
    }

    switch (aliasType) {
        case PT_LOOKUP: {
            SymbolRecord *arraySymbol = lookupSymbolNode(alias->nodes[1], alias->lineNum);
            SymbolRecord *indexSymbol = lookupSymbolNode(alias->nodes[2], alias->lineNum);

            char multiplier = (char) calcVarSize(structSymbol);
            ICG_MultiplyVarWithConst(indexSymbol, multiplier & 0x7f);
            ICG_MoveAccToIndex('Y');

            // Make sure instruction generator knows the use, so we don't have to keep reloading
            ICG_Tag('Y', structSymbol);
        } break;

        case PT_INIT:
            // nothing to load
            break;
        default:
            ErrorMessageWithList("-Invalid alias:", structSymbol->alias);
            ErrorMessageWithList("-Need to valid alias for expression", expr);
            break;
    }
    return aliasType;
}

SymbolRecord *GC_GetAliasBase(const List *expr, SymbolRecord *structSymbol, enum ParseToken aliasType) {
    List *alias = structSymbol->alias;
    switch (aliasType) {
        case PT_LOOKUP:
        case PT_INIT:
            return lookupSymbolNode(alias->nodes[1], alias->lineNum);
        default:
            return NULL;
    }
}

void GC_LoadPropertyRef(const List *expr, enum SymbolType destType) {
    if ((expr->nodes[1].type != N_STR) || (expr->nodes[2].type != N_STR)) {
        ErrorMessageWithList("GC_LoadPropertyRef cannot handle",expr);
        return;
    }

    char *structSymbolName = expr->nodes[1].value.str;
    char *propName = expr->nodes[2].value.str;

    SymbolRecord *structSymbol = lookupSymbolNode(expr->nodes[1], expr->lineNum);
    if (!structSymbol) return;      /// EXIT if invalid structure

    // Handle enumeration reference
    if (isEnum(structSymbol)) {
        EvalResult evalResult = evaluate_expression(expr);
        if (evalResult.hasResult) {
            ICG_LoadConst(evalResult.value, 1);
        } else {
            ErrorMessageWithList("Unable to process enumeration reference", expr);
        }
        return;
    }


    SymbolRecord *propertySymbol = lookupProperty(structSymbol, propName);
    if (!propertySymbol) {
        ErrorMessageWithList("GC_LoadPropertyRef: Invalid property reference", expr);
        return;      /// EXIT if invalid structure property
    }

    // Now we can get down to business!
    int propertyOffset = GET_PROPERTY_OFFSET(propertySymbol);

    if (isStructDefined(structSymbol) && IS_PARAM_VAR(structSymbol)) {

        IL_AddCommentToCode("using GC_LoadPropertyRef -- is param");

        ICG_LoadRegConst('Y', propertyOffset);
        ICG_LoadIndirect(structSymbol, 0);

    } else if (IS_ALIAS(structSymbol)) {
        enum ParseToken aliasType = GC_LoadAlias(expr, structSymbol);
        SymbolRecord *baseSymbol = GC_GetAliasBase(expr, structSymbol, aliasType);
        if (baseSymbol != NULL) {
            if (aliasType == PT_LOOKUP) {
                ICG_LoadIndexedWithOffset(baseSymbol, propertyOffset);
            } else {
                ICG_LoadPropertyVar(baseSymbol, propertySymbol);
            }
        } else {
            ErrorMessage("Cannot load property via alias", structSymbol->name, expr->lineNum);
        }
    } else {
        ICG_LoadPropertyVar(structSymbol, propertySymbol);
    }
}



//-------------------------------------------------------------------------
//---  Core of Expression Operators
//-------------------------------------------------------------------------


void GC_SimpleOP(const List *expr, enum MnemonicCode mne, enum SymbolType destType) {
    ListNode arg = expr->nodes[1];
    switch (arg.type) {
        case N_STR: {
            SymbolRecord *varSym = lookupSymbolNode(arg, expr->lineNum);
            if (varSym != NULL) {
                int destSize = (destType == ST_INT || destType == ST_PTR) ? 2 : 1;
                ICG_OpWithVar(mne, varSym, destSize);
            }
        } break;
        case N_LIST:
            GC_Expression(arg.value.list, destType);
            break;
        default:
            ErrorMessageWithList("Invalid SimpleOp arg: ", expr);
    }
}

void GC_OpWithArrayElement(enum MnemonicCode mne, const List *expr) {
    SymbolRecord *arraySym = getArraySymbol(expr, expr->nodes[1]);
    if (isArrayIndexConst(expr)) {
        int ofs = GC_GetArrayIndex(arraySym, expr);
        ICG_OpWithOffset(mne, arraySym, ofs);
    } else {
        // Array Index is an expression or a variable
        SymbolRecord *indexSym = lookupSymbolNode(expr->nodes[2], expr->lineNum);
        ICG_OpIndexed(mne, arraySym, indexSym);
    }
}

// TODO: This could be done a better way... BUT This is way nicer than was previously done
//         There's a bit of redundancy between the call out to type check and the two symbol lookups.

void GC_OpWithPropertyRef(enum MnemonicCode mne, const List *expr, enum SymbolType destType) {
    if (TypeCheck_PropertyReference(expr, destType)) {
        SymbolRecord *structSym = lookupSymbolNode(expr->nodes[1], expr->lineNum);

        // Handle enumeration reference
        if (isEnum(structSym)) {
            EvalResult evalResult = evaluate_expression(expr);
            if (evalResult.hasResult) {
                ICG_OpWithConst(mne, evalResult.value, 1);
            } else {
                ErrorMessageWithList("Unable to process enumeration reference", expr);
            }
            return;
        }

        //---------------------------------------------------------
        // Not an enumeration, process as regular struct access

        SymbolRecord *propSym = findSymbol(getStructSymbolSet(structSym), expr->nodes[2].value.str);
        if (IS_ALIAS(structSym)) {
            enum ParseToken aliasType = GC_LoadAlias(expr, structSym);
            SymbolRecord *baseSymbol = GC_GetAliasBase(expr, structSym, aliasType);
            if (aliasType == PT_LOOKUP) {
                ICG_OpPropertyVarIndexed(mne, baseSymbol, propSym);
            } else {
                ICG_OpPropertyVar(mne, baseSymbol, propSym);
            }
        } else {
            ICG_OpPropertyVar(mne, structSym, propSym);
        }
    }
}


bool isSimpleArrayRef(ListNode arg2) {
    return (arg2.type == N_LIST)
                && isToken(arg2.value.list->nodes[0], PT_LOOKUP)
                && (arg2.value.list->hasNestedList == false);
}

bool isSimplePropertyRef(ListNode arg2) {
    return (arg2.type == N_LIST)
               && isToken(arg2.value.list->nodes[0], PT_PROPERTY_REF)
               && (arg2.value.list->hasNestedList == false);
}

bool isSimpleArg(ListNode arg1) {
    return ((arg1.type == N_INT) || (arg1.type == N_STR)) || isSimplePropertyRef(arg1);
}

void GC_OP(const List *expr, enum MnemonicCode mne, enum SymbolType destType, enum MnemonicCode preOp) {
    int destSize = (destType == ST_INT || destType == ST_PTR) ? 2 : 1;

    ListNode arg1 = expr->nodes[1];
    ListNode arg2 = expr->nodes[2];

    // if second arg is a list and first is not,
    //   swap order of arguments if order of args for the given operation doesn't matter
    bool isInterchangeableOp = ((mne == AND) || (mne == ORA) || (mne == EOR));
    if (isInterchangeableOp && (arg2.type == N_LIST) && isSimpleArg(arg1)) {
        ListNode temp = arg1;
        arg1 = arg2;
        arg2 = temp;
    }

    GC_HandleLoad(arg1, destType, expr->lineNum);

    //------ now do op using arg2

    if (arg2.type == N_INT) {
        ICG_PreOp(preOp);
        ICG_OpWithConst(mne, arg2.value.num, destSize);
    } else if (arg2.type == N_STR) {
        SymbolRecord *varRec = lookupSymbolNode(arg2, expr->lineNum);
        if (varRec != NULL) {
            ICG_PreOp(preOp);

            ICG_OpWithVar(mne, varRec, destSize);
        } else {
            ErrorMessage("Unknown argument to op", arg2.value.str, expr->lineNum);
        }
    } else if (isSimplePropertyRef(arg2)) {
        List *propertyRef = arg2.value.list;
        SymbolRecord *structSymbol = lookupSymbolNode(propertyRef->nodes[1], expr->lineNum);

        // check for aliased structure
        if (structSymbol && IS_ALIAS(structSymbol)) {
            SymbolRecord *propertySymbol = findSymbol(getStructSymbolSet(structSymbol),
                                                      propertyRef->nodes[2].value.str);
            if (!propertySymbol) {
                ErrorMessageWithList("GC_Op: Invalid property reference", propertyRef);
                return;      /// EXIT if invalid structure property
            }

            ICG_PreOp(preOp);

            enum ParseToken aliasType = GC_LoadAlias(propertyRef, structSymbol);
            SymbolRecord *baseSymbol = GC_GetAliasBase(expr, structSymbol, aliasType);
            if (aliasType == PT_LOOKUP) {
                ICG_OpIndexedWithOffset(mne, baseSymbol, GET_PROPERTY_OFFSET(propertySymbol));
            } else {
                ICG_OpPropertyVar(mne, baseSymbol, propertySymbol);
            }
        } else {
            ICG_PreOp(preOp);
            GC_OpWithPropertyRef(mne, propertyRef, destType);
        }

    } else if (isSimpleArrayRef(arg2)) {
        List *arrayRef = arg2.value.list;
        ICG_PreOp(preOp);
        GC_OpWithArrayElement(mne, arrayRef);

    } else if (arg2.type == N_LIST) {

        // first check to see if expression can be completely evaluated
        EvalResult evalResult = evaluate_expression(arg2.value.list);

        if (evalResult.hasResult) {
            bool isWord = ((destType == ST_INT) || (destType == ST_PTR));
            int dataSize = isWord ? 2 : 1;
            ICG_PreOp(preOp);
            ICG_OpWithConst(mne, evalResult.value, dataSize);
        } else {
            ICG_PushAcc();
            GC_Expression(arg2.value.list, destType);
            ICG_PreOp(preOp);
            ICG_OpWithStack(mne);
        }
    }
}



//---------------------------------------------------------
//
//    Generate Code from the Abstract Parse Tree
//
//---------------------------------------------------------

void GC_Inc(const List *expr, enum SymbolType destType) { GC_SimpleOP(expr, INC, destType); }
void GC_Dec(const List *expr, enum SymbolType destType) { GC_SimpleOP(expr, DEC, destType); }
void GC_And(const List *expr, enum SymbolType destType) { GC_OP(expr, AND, destType, MNE_NONE); }
void GC_Or(const List *expr, enum SymbolType destType) { GC_OP(expr, ORA, destType, MNE_NONE); }
void GC_Eor(const List *expr, enum SymbolType destType) { GC_OP(expr, EOR, destType, MNE_NONE); }
void GC_Add(const List *expr, enum SymbolType destType) {
    GC_OP(expr, ADC, destType, CLC);
}
void GC_Sub(const List *expr, enum SymbolType destType) {
    GC_OP(expr, SBC, destType, SEC);
}
void GC_BoolAnd(const List *expr, enum SymbolType destType) {
    GC_OP(expr, AND, destType, MNE_NONE);
}
void GC_BoolOr(const List *expr, enum SymbolType destType) {
    GC_OP(expr, ORA, destType, MNE_NONE);
}

void GC_Not(const List *expr, enum SymbolType destType) {
    ListNode notNode = expr->nodes[1];
    if (notNode.type == N_STR) {
        SymbolRecord *varSym = lookupSymbolNode(notNode, expr->lineNum);
        if (varSym != NULL && getType(varSym) == ST_BOOL) {
            GC_LoadVar(notNode, expr->lineNum);
            ICG_NotBool();
            return;
        }
    }
    GC_HandleLoad(expr->nodes[1], destType, expr->lineNum);
    ICG_Not();
}

void GC_Negate(const List *expr, enum SymbolType destType) {
    GC_HandleLoad(expr->nodes[1], destType, expr->lineNum);
    ICG_Negate();
}

bool isGoodShiftCount(ListNode countNode) {
    return ((countNode.type == N_INT) &&
            (countNode.value.num > 0) &&
            (countNode.value.num < 16));
}

SymbolRecord *getShiftDestSym(ListNode varNode, int lineNum) {
    SymbolRecord *result = NULL;
    if (varNode.type == N_STR) {
        result = lookupSymbolNode(varNode, lineNum);
    } else if (varNode.type == N_LIST) {
        GC_Expression(varNode.value.list, ST_NONE);
    }
    return result;
}

void GC_ShiftLeft(const List *expr, enum SymbolType destType) {
    SymbolRecord *shiftDest = getShiftDestSym(expr->nodes[1], expr->lineNum);

    ListNode countNode = expr->nodes[2];
    if (isGoodShiftCount(countNode)) {
        ICG_ShiftLeft(shiftDest, countNode.value.num);
    } else {
        ErrorMessageWithList("Unsupported form of Shift left: ", expr);
    }
}

void GC_ShiftRight(const List *expr, enum SymbolType destType) {
    SymbolRecord *shiftDest = getShiftDestSym(expr->nodes[1], expr->lineNum);
    ListNode countNode = expr->nodes[2];
    if (isGoodShiftCount(countNode)) {
        ICG_ShiftRight(shiftDest, countNode.value.num);
    } else {
        ErrorMessageWithList("Unsupported form of Shift right: ", expr);
    }
}


bool GC_SimpleModifyOp(enum MnemonicCode mne, const List *stmt, enum SymbolType destType) {
    ListNode arg = stmt->nodes[1];
    if (arg.type == N_LIST) {
        List *expr = arg.value.list;
        switch (expr->nodes[0].value.parseToken) {
            case PT_LOOKUP:
                GC_OpWithArrayElement(mne, expr);
                break;
            case PT_PROPERTY_REF:
                GC_OpWithPropertyRef(mne, expr, destType);
                break;
            default:
                return false;
        }
    } else if (arg.type == N_STR) {
        // This is generally an INC/DEC op with a variable

        SymbolRecord *varSym = lookupSymbolNode(arg, stmt->lineNum);
        if (varSym != NULL) {
            ICG_OpWithVar(mne, varSym, getBaseVarSize(varSym));
        }
    }
    return true;
}

void GC_IncStmt(const List *stmt, enum SymbolType destType) {
    bool success = GC_SimpleModifyOp(INC, stmt, destType);
    if (!success) {
        ErrorMessageWithList("Invalid increment statement", stmt);
    }
}

void GC_DecStmt(const List *stmt, enum SymbolType destType) {
    bool success = GC_SimpleModifyOp(DEC, stmt, destType);
    if (!success) {
        ErrorMessageWithList("Invalid decrement statement", stmt);
    }
}

/**
 * Simple function for determining which branch opcode to use.
 *
 * @param opNode    - node containing the comparison operation (parseToken) to convert to branch op
 * @param skipLabel - destination of branch op
 * @param isCmpToZeroR - is this a comparison to zero?
 * @param isSignedCmp - is this a signed or unsigned comparison?
 */
void GC_HandleBranchOp(ListNode opNode, const Label *skipLabel, bool isCmpToZeroR,
                       bool isSignedCmp) {
    // now select which branch statement to use
    switch (opNode.value.parseToken) {
        case PT_EQ: ICG_Branch(BNE, skipLabel); break;
        case PT_NE: ICG_Branch(BEQ, skipLabel); break;
        case PT_LTE:
            if (isSignedCmp) {
                ICG_Branch(BPL, skipLabel);     // signed numbers
            } else {
                ICG_Branch(BCS, skipLabel);     // unsigned numbers
            }
            ICG_Branch(BEQ, skipLabel);
            break;
        case PT_LT:
            if (isCmpToZeroR || isSignedCmp) {
                ICG_Branch(BPL, skipLabel);        // signed numbers
            } else {
                ICG_Branch(BCS, skipLabel);      // unsigned numbers
            }
            break;
        case PT_GTE:
            if (isCmpToZeroR || isSignedCmp) {
                ICG_Branch(BMI, skipLabel);        // signed numbers
            } else {
                ICG_Branch(BCC, skipLabel);      // unsigned numbers
            }
            break;
        case PT_GT:
            if (!isCmpToZeroR) {
                if (isSignedCmp) {
                    ICG_Branch(BMI, skipLabel);     // signed numbers
                } else {
                    ICG_Branch(BCC, skipLabel);     // unsigned numbers
                }
            }
            ICG_Branch(BEQ, skipLabel);
            break;
        default:
            break;
    }
}


//---------------------------------------------------------------------------------------
//   Handle type casting
//

void GC_Cast(const List *expr, enum SymbolType destType) {
    ListNode castInfo = expr->nodes[1];
    ListNode castExpr = expr->nodes[2];

    enum SymbolType castType = destType;
    /*if (castExpr)

    // TODO: cast info should be compared against destination type
    if (castType != destType) {
        ErrorMessageWithList("Illegal cast operation", expr);
        return;
    }*/

    GC_Expression(castExpr.value.list, castType);
}


//------------------------------------------------------------------------------------
// Address operator - Get address of variable (for pointer creation/manipulation)
//
//   &varName                --  [addrOf, varName]
//
void GC_AddrOf(const List *expr, enum SymbolType destType) {
    ListNode addrNode = expr->nodes[1];
    if (addrNode.type == N_LIST) {
        ErrorMessageWithList("Expression not allowed with Address Of operator", expr);
    } else if (addrNode.type == N_STR) {
        SymbolRecord *varSym = lookupSymbolNode(addrNode, expr->lineNum);
        if (varSym != NULL) {
            ICG_LoadAddr(varSym);
        }
    }
}




void GC_CompareOp(const List *expr, enum SymbolType destType) {
    ListNode opNode = expr->nodes[0];
    Label *skipLabel = newGenericLabel(L_CODE);

    // TODO: maybe need to do something different if 16-bit operation
    ICG_LoadRegConst('X', 0);
    GC_OP(expr, CMP, destType, MNE_NONE);
    //ICG_Branch(BNE, skipLabel);  // skip if not equal  (for EQ op)
    GC_HandleBranchOp(opNode, skipLabel, false, false);
    ICG_PreOp(INX);
    IL_Label(skipLabel);
    ICG_PreOp(TXA);
}


/**
 * Generate code for a multiplication operation
 *
 * REMEMBER:
 *   'Commutative property: The order of the factors does not change the product.'
 *
 * Cases:
 *    NUM * NUM => should be handled in the evaluator (never should make it here)
 *    VAR * NUM ->  Multiply With Const
 *    VAR * VAR ->  Multiply Var with Var
 *    EXPR * NUM -> Process Expr, then Multiply Temp Var with Const
 *    EXPR * VAR -> Process Expr, then Multiply Temp Var with Var
 *    EXPR * EXPR -> Process both expr, store in temps, Multiply Temp Var with Temp Var  (requires 2 temp vars)
 *
 * @param expr
 * @param destType
 */
void GC_MultiplyOp(const List *expr, enum SymbolType destType) {
    ListNode firstParam = expr->nodes[1];
    ListNode secondParam = expr->nodes[2];

    //--------------------------------------------------------------
    //  Replace a const var with it's value if possible

    if (isConstValueNode(firstParam, expr->lineNum)) {
        firstParam = createIntNode(getConstValue(firstParam, expr->lineNum));
    }
    if (isConstValueNode(secondParam, expr->lineNum)) {
        secondParam = createIntNode(getConstValue(secondParam, expr->lineNum));
    }

    //--------------------------------------------------------------
    // swap params if first one is less important than second
    ListNode tempNode;

    // demote numeric literal to second param
    if (firstParam.type == N_INT && secondParam.type != N_INT) {
        tempNode = secondParam;
        secondParam = firstParam;
        firstParam = tempNode;
    }

    // promote expr to first position IF first position is not already an expr
    if (firstParam.type != N_LIST && secondParam.type == N_LIST) {
        tempNode = secondParam;
        secondParam = firstParam;
        firstParam = tempNode;
    }

    //--------------------------------------------------------------
    // handle case when first param is a sub-expression

    SymbolRecord *varSym;
    switch (firstParam.type) {

        // handle if the first parameter is an expression... second param can be VAR or INT
        case N_LIST:
            GC_Expression(firstParam.value.list, destType);
            if (secondParam.type == N_STR) {
                varSym = lookupSymbolNode(secondParam, expr->lineNum);
                ICG_MultiplyExprWithVar(0, varSym);        // first param (currently unused) is temp var location
            } else if (secondParam.type == N_INT) {
                ICG_MultiplyWithConst(secondParam.value.num);
            }
            break;

        // handle if first parameter is a variable
        case N_STR:
            varSym = lookupSymbolNode(firstParam, expr->lineNum);
            if (varSym == NULL) return;

            if (secondParam.type == N_INT) {
                ICG_MultiplyVarWithConst(varSym, secondParam.value.num);
            } else if (secondParam.type == N_STR) {
                SymbolRecord *varSym2 = lookupSymbolNode(secondParam, expr->lineNum);
                ICG_MultiplyVarWithVar(varSym, varSym2);
            } else {
                ErrorMessageWithNode("Multiply op not supported", secondParam, expr->lineNum);
            }
            break;

        // we should never end up here... so report an error
        default:
            ErrorMessageWithList("Multiply op not supported using the following:", expr);
    }
}

//---------------------------------------------------------------------------------

ParseFuncTbl exprFunction[] = {
        {PT_PROPERTY_REF,   &GC_LoadPropertyRef},
        {PT_ADDR_OF,        &GC_AddrOf},
        {PT_LOOKUP,         &GC_Lookup},
        {PT_BIT_AND,        &GC_And},
        {PT_BIT_OR,         &GC_Or},
        {PT_BIT_EOR,        &GC_Eor},
        {PT_CAST,           &GC_Cast},
        {PT_INC,            &GC_Inc},
        {PT_DEC,            &GC_Dec},
        {PT_ADD,            &GC_Add},
        {PT_NEGATIVE,       &GC_Negate},
        {PT_SUB,            &GC_Sub},
        {PT_SHIFT_LEFT,     &GC_ShiftLeft},
        {PT_SHIFT_RIGHT,    &GC_ShiftRight},
        {PT_BOOL_AND,       &GC_BoolAnd},
        {PT_BOOL_OR,        &GC_BoolOr},
        {PT_NOT,            &GC_Not},
        {PT_FUNC_CALL,      &GC_FuncCallExpression},
        {PT_EQ,             &GC_CompareOp},
        {PT_NE,             &GC_CompareOp},
        {PT_GT,             &GC_CompareOp},
        {PT_LT,             &GC_CompareOp},
        {PT_GTE,            &GC_CompareOp},
        {PT_LTE,            &GC_CompareOp},
        {PT_MULTIPLY,       &GC_MultiplyOp},
};
const int exprFunctionSize = sizeof(exprFunction) / sizeof(ParseFuncTbl);


void GC_Expression(const List *expr, enum SymbolType destType) {
    ListNode opNode = expr->nodes[0];
    bool isExprHandled = false;
    if (opNode.type == N_TOKEN) {

        //  First evaluate the expression to catch and preprocess any simple constant expressions

        EvalResult evalResult = evaluate_expression(expr);
        if (evalResult.hasResult) {
            // expression was evaluated and produced a numeric result, so we can just use the resulting value
            bool isWord = ((destType == ST_INT) || (destType == ST_PTR));
            int destSize = isWord ? 2 : 1;

            IL_SetLineComment(get_expression(expr));
            //IL_SetLineComment(isWord ? "word" : "byte");

            ICG_LoadConst(evalResult.value, destSize);
            isExprHandled = true;
        } else {
            // expression needs to be processed and have code generated

            int funcIndex = findFunc(exprFunction, exprFunctionSize, opNode.value.parseToken);
            if (funcIndex >= 0) {
                exprFunction[funcIndex].parseFunc(expr, destType);
                isExprHandled = true;
            }
        }
    }

    if (!isExprHandled) {
        ErrorMessageWithList("Expression not implemented\n", expr);
    }
}

void GC_StoreToStructProperty(const List *expr) {
    ListNode structNameNode = expr->nodes[1];
    char *propName = expr->nodes[2].value.str;

    SymbolRecord *structSym = lookupSymbolNode(structNameNode, expr->lineNum);
    if (structSym == NULL || !isStructDefined(structSym)) return;

    SymbolRecord *propertySym = findSymbol(getStructSymbolSet(structSym), propName);
    if (propertySym == NULL) return;

    int propertyOffset = GET_PROPERTY_OFFSET(propertySym);

    if (IS_ALIAS(structSym)) {

        enum ParseToken aliasType = GC_LoadAlias(expr, structSym);
        SymbolRecord *baseSymbol = GC_GetAliasBase(expr, structSym, aliasType);
        if (aliasType == PT_LOOKUP) {
            ICG_StoreIndexedWithOffset(baseSymbol, propertyOffset);
        } else if (aliasType == PT_INIT) {
            ICG_StoreVarOffset(baseSymbol, propertyOffset, getBaseVarSize(propertySym));
        }

    } else {
        ICG_StoreVarOffset(structSym, propertyOffset, getBaseVarSize(propertySym));
    }
}

void GC_StoreToArray(const List *expr) {
    SymbolRecord *arraySym = getArraySymbol(expr, expr->nodes[1]);
    IL_SetLineComment(arraySym->name);

    if (isArrayIndexConst(expr)) {
        // Array index is a constant value (either a const variable or a numeric literal)
        int ofs = GC_GetArrayIndex(arraySym, expr);
        ICG_StoreVarOffset(arraySym, ofs, getBaseVarSize(arraySym));
        return;

    } else if (expr->nodes[2].type == N_STR) {
        // Array Index is an expression or a variable
        SymbolRecord *varSym = lookupSymbolNode(expr->nodes[2], expr->lineNum);
        if (varSym) ICG_LoadIndexVar(varSym, getBaseVarSize(varSym));
        ICG_StoreVarIndexed(arraySym);
        return;

    } else if (expr->nodes[2].type == N_LIST) {
        List *arrayIndexExpr = expr->nodes[2].value.list;
        ListNode arg1 = arrayIndexExpr->nodes[1];
        ListNode arg2 = arrayIndexExpr->nodes[2];

        if ((arg1.type == N_INT) && (arg2.type == N_STR)) {
            arg2 = arrayIndexExpr->nodes[1];
            arg1 = arrayIndexExpr->nodes[2];
        }

        printf("Processing store to array with var + const\n");

        if (arg2.type == N_INT) {
            int ofs = arg2.value.num;

            SymbolRecord *varSym = lookupSymbolNode(arg1, expr->lineNum);
            if (!varSym) return;
            ICG_LoadIndexVar(varSym, getBaseVarSize(varSym));

            switch (arrayIndexExpr->nodes[0].value.parseToken) {
                case PT_ADD:
                    ICG_StoreIndexedWithOffset(arraySym, ofs);
                    return;

                case PT_SUB:
                    ICG_StoreIndexedWithOffset(arraySym, -ofs);
                    return;

                default:
                    break;
            }
        }
    }

    ErrorMessageWithNode("Unsupported array index", expr->nodes[2], expr->lineNum);
}

/*** Process Storage Expression - (Left-side of assignment) */
void GC_ExpressionForStore(const List *expr, enum SymbolType destType) {
    ListNode opNode = expr->nodes[0];
    if (opNode.type == N_TOKEN) {
        switch (opNode.value.parseToken) {
            case PT_PROPERTY_REF:  GC_StoreToStructProperty(expr);  break;
            case PT_LOOKUP:        GC_StoreToArray(expr);           break;
            case PT_INC:           GC_Inc(expr, ST_NONE);           break;
            case PT_DEC:           GC_Dec(expr, ST_NONE);           break;
        }
    } else {
        ErrorMessageWithNode("Invalid token in assignment expr\n", expr->nodes[0], expr->lineNum);
    }
}


enum SymbolType getVarDestType(SymbolRecord *destVar) {
    return isPointer(destVar)
               ? ST_PTR
               : getType(destVar);
}

SymbolRecord *getAsgnDestVarSym(const List *stmt, ListNode storeNode) {
    SymbolRecord *destVar = NULL;
    switch (storeNode.type) {
        case N_STR:
            return lookupSymbolNode(storeNode, stmt->lineNum);

        case N_LIST: {
            List *storeExpr = storeNode.value.list;
            // probably a property ref or an array lookup
            if (isToken(storeExpr->nodes[0], PT_LOOKUP)) {
                return lookupSymbolNode(storeExpr->nodes[1], stmt->lineNum);

            } else if (isToken(storeExpr->nodes[0], PT_PROPERTY_REF)) {

                char *propName = storeExpr->nodes[2].value.str;
                SymbolRecord *structVar = lookupSymbolNode(storeExpr->nodes[1], stmt->lineNum);
                if (structVar != NULL) {
                    SymbolRecord *propertySym = findSymbol(getStructSymbolSet(structVar), propName);
                    destVar = propertySym;
                    if (propertySym == NULL) {
                        ErrorMessageWithList("Property not found", storeExpr);
                    } else if (IS_ALIAS(structVar)) {
                        GC_LoadAlias(storeExpr, structVar);
                    }
                }
            } else {
                ErrorMessageWithList("Cannot determine destination type from storage expression", storeExpr);
            }
        } break;
    }
    return destVar;
}

/**
 * Process assignment statement
 *
 * @param stmt - statement to process
 * @param destType - unused for now.  This gets overridden by a call to getAsgnDestType
 */
void GC_Assignment(const List *stmt, enum SymbolType destType) {
    ListNode loadNode = stmt->nodes[2];
    ListNode storeNode = stmt->nodes[1];

    // figure out destination type
    SymbolRecord *destVar = getAsgnDestVarSym(stmt, storeNode);
    destType = (destVar != NULL) ? getVarDestType(destVar) : ST_ERROR;

    //printf("DEBUG: GC_Assignment -> getAsgnDestType = %d\n", destType);

    // make sure we have a destination
    if (destType != ST_ERROR) {

        /*
        // flatten out the expression before code generation
        if (loadNode.type == N_LIST) {
            flatten_expression(loadNode.value.list, stmt);
        }
        */

        // figure out where data is coming from
        GC_HandleLoad(loadNode, destType, stmt->lineNum);

        //--------------------------------------------------------------
        // At this point A register should have data to store...
        //    just need to calculate destination address
        //    and then store the data
        switch (storeNode.type) {
            case N_STR:
                GC_StoreInVar(storeNode, destType, stmt->lineNum);
                break;
            case N_LIST:
                GC_ExpressionForStore(storeNode.value.list, destType);
                break;
        }
    } else {
        printf("#ERROR# destination type unknown\n");
    }
}

void GC_Strobe(const List *stmt, enum SymbolType destType) {
    ListNode storeNode = stmt->nodes[1];
    switch (storeNode.type) {
        case N_LIST: GC_ExpressionForStore(storeNode.value.list, destType); break;
        case N_STR:  GC_StoreInVar(storeNode, destType, stmt->lineNum); break;
        case N_INT:  ICG_StoreToAddr(storeNode.value.num, 1); break;
    }
}

void GC_Return(const List *stmt, enum SymbolType destType) {
    ListNode returnExprNode = stmt->nodes[1];
    switch (returnExprNode.type) {
        case N_LIST: GC_Expression(returnExprNode.value.list, destType); break;
        case N_STR:  ICG_LoadVar(lookupSymbolNode(returnExprNode, stmt->lineNum)); break;
        case N_INT:  ICG_LoadConst(returnExprNode.value.num, 0); break;
    }
    ICG_Return();
}


//===============================================================================
//==== Expression Type Matching / Type upgrading

enum SymbolType getSrcType(const ListNode argNode, int lineNum) {
    enum SymbolType srcType = ST_CHAR;
    SymbolRecord *varSym;
    int value;
    switch (argNode.type) {
        case N_INT:
            value = argNode.value.num;
            if ((value > 256) || (value < -128)) srcType = ST_INT;
            if (value < 0) srcType = srcType | ST_SIGNED;
            break;

        case N_STR:
            varSym = lookupSymbolNode(argNode, lineNum);
            if (varSym) srcType = getType(varSym);
            break;

        default:break;
    }
    return srcType;
}

enum SymbolType getExprType(const ListNode arg1, const ListNode arg2, int lineNum) {
    enum SymbolType arg1Type = getSrcType(arg1, lineNum);
    enum SymbolType arg2Type = getSrcType(arg2, lineNum);

    if (arg1Type == arg2Type) return arg1Type;

    bool isSigned = false;
    if ((arg1Type >= ST_SIGNED) || (arg2Type >= ST_SIGNED)) {
        arg1Type &= ST_MASK;
        arg2Type &= ST_MASK;
        isSigned = true;
    }

    enum SymbolType exprType = ST_CHAR;
    if (arg1Type >= arg2Type) exprType = arg1Type;
    if (arg1Type < arg2Type) exprType = arg2Type;
    return isSigned ? (exprType | ST_SIGNED) : exprType;
}


/* ------------------------------------------------------------------ */
/*
 *   Conditional expression and comparison statement handling
 *
 * */


/**
 * Handle If conditional expression separately for optimization purposes
 * @param expr
 */

void GC_Compare(ListNode arg, int lineNum) {
    switch (arg.type) {
        case N_INT: ICG_CompareConst(arg.value.num); break;
        case N_STR: {
            SymbolRecord *varSym = lookupSymbolNode(arg, lineNum);
            if (varSym != NULL) ICG_CompareVar(varSym);
        } break;
        case N_LIST: {
            // attempt to evaluate expression here
            EvalResult evalResult = evaluate_expression(arg.value.list);
            if (evalResult.hasResult) {
                IL_SetLineComment(get_expression(arg.value.list));
                ICG_CompareConst(evalResult.value);
            } else {
                ErrorMessageWithList("Cannot evaluate for compare: ", arg.value.list);
            }
        } break;
        default:
            ErrorMessageWithNode("Unsupported compare op: ", arg, lineNum);
    }
}



void GC_HandleSubCondExpr(const ListNode ifExprNode, enum SymbolType destType, Label *skipLabel) {
    if (ifExprNode.type == N_LIST) {
        List *expr = ifExprNode.value.list;
        ListNode opNode = expr->nodes[0];
        ListNode arg1 = expr->nodes[1];
        ListNode arg2 = expr->nodes[2];

        if (opNode.type == N_TOKEN) {

            // optimizations for booleans
            if (opNode.value.parseToken == PT_BOOL_AND) {

                GC_HandleLoad(arg1, ST_NONE, expr->lineNum);
                ICG_Branch(BEQ, skipLabel);    // shortcut boolean logic

                GC_HandleLoad(arg2, ST_NONE, expr->lineNum);
                ICG_Branch(BEQ, skipLabel);

            } else if (isComparisonToken(opNode.value.parseToken)) {
                // handle as normal comparison
                bool isCmpToZeroR = (arg2.type == N_INT && arg2.value.num == 0);
                bool isSignedCmp = false;

                // load first argument, and compare to second
                GC_HandleLoad(arg1, ST_NONE, expr->lineNum);
                if (!isCmpToZeroR) {
                    GC_Compare(arg2, expr->lineNum);
                }
                GC_HandleBranchOp(opNode, skipLabel, isCmpToZeroR, isSignedCmp);
            } else {
                //----- if no comparison operators are used, eval expr, skip code if 0
                GC_Expression(expr, destType);
                ICG_Branch(BEQ, skipLabel);
            }
        } else {
            ErrorMessageWithList("IfExpr: not implemented", expr);
        }
    } else if (ifExprNode.type == N_STR) {
        ICG_Branch(BEQ, skipLabel);
    }
}


void GC_HandleBasicCompareOp(ListNode opNode, ListNode arg1, ListNode arg2, const Label *skipLabel,
                        const List *expr) {
    bool isCmpToZeroR = (arg2.type == N_INT && arg2.value.num == 0);
    bool isSignedCmp = (getExprType(arg1, arg2, expr->lineNum) & ST_SIGNED);

    // load first argument, and compare to second
    GC_HandleLoad(arg1, ST_NONE, expr->lineNum);
    if (!isCmpToZeroR) {
        GC_Compare(arg2, expr->lineNum);
    }
    GC_HandleBranchOp(opNode, skipLabel, isCmpToZeroR, isSignedCmp);
}

/**
 * Handle a conditional expression (typically part of if, while, etc...)
 * @param ifExprNode
 * @param destType
 * @param skipLabel
 */
void GC_HandleCondExpr(const ListNode ifExprNode, enum SymbolType destType, Label *skipLabel, int lineNum) {

    // check basic case of (identifier)
    if (ifExprNode.type == N_STR) {
        //printf("GC_HandleCondExpr: using basic case on line %d\n", lineNum);
        ICG_LoadVar(lookupSymbolNode(ifExprNode, lineNum));
        ICG_Branch(BEQ, skipLabel);
        return;
    }

    // handle constant
    if (ifExprNode.type == N_INT) {
        if (ifExprNode.value.num == 0) ICG_Jump(skipLabel, "skipping code");
    }

    // only other thing supported is list, so leave if not a list
    if (ifExprNode.type != N_LIST) return;

    List *expr = ifExprNode.value.list;
    ListNode opNode = expr->nodes[0];
    ListNode arg1 = expr->nodes[1];
    ListNode arg2 = expr->nodes[2];

    if (opNode.type != N_TOKEN) {
        ErrorMessageWithList("IfExpr: not implemented", expr);
        return;
    }

    // optimizations for booleans
    if (opNode.value.parseToken == PT_BOOL_AND) {
        GC_HandleSubCondExpr(arg1, ST_NONE, skipLabel);
        GC_HandleSubCondExpr(arg2, ST_NONE, skipLabel);
    } else if (isComparisonToken(opNode.value.parseToken)) {
        GC_HandleBasicCompareOp(opNode, arg1, arg2, skipLabel, expr);
    } else {
        //----- if no comparison operators are used, eval expr, skip code if 0
        GC_Expression(expr, destType);
        ICG_Branch(BEQ, skipLabel);
    }
}


//-------------------------------------------------------------------------------
//  Statements

void GC_If(const List *stmt, enum SymbolType destType) {
    bool hasElse = (stmt->count > 3) && (stmt->nodes[3].type == N_LIST);

    Label *skipThenLabel = newGenericLabel(L_CODE);
    Label *skipElseLabel = (hasElse ? newGenericLabel(L_CODE) : NULL);

    // handle if:
    GC_HandleCondExpr(stmt->nodes[1], ST_BOOL, skipThenLabel, stmt->lineNum);

    // handle then:
    GC_CodeBlock(stmt->nodes[2].value.list);

    // handle else:
    if (hasElse) {
        ICG_Jump(skipElseLabel, "skip else case");
        IL_Label(skipThenLabel);

        GC_CodeBlock(stmt->nodes[3].value.list);
        IL_Label(skipElseLabel);
    } else {
        IL_Label(skipThenLabel);
    }
}


//------
//    [switch, [expr],
//        [case, value, [code]],
//        [case, value, [code]],
//        ...,
//        [default, [code]] ]

const char *INVALID_CASE_ENUM_ERROR = "Unable to process enumeration reference for case statement";

bool isVarEnum(SymbolRecord *varSym) {
    return (varSym && varSym->userTypeDef && isEnum(varSym->userTypeDef));
}

void GC_HandleCase(const List *caseStmt, SymbolRecord *switchVarSymbol) {
    EvalResult evalResult;
    ListNode caseValueNode = caseStmt->nodes[1];

    switch (caseValueNode.type) {
        case N_INT:
            ICG_CompareConst(caseValueNode.value.num);
            return;
            // ^^^ - done with handling case statement, need to exit now

        case N_STR:
            if (isVarEnum(switchVarSymbol)) {
                evalResult = evaluate_enumeration(switchVarSymbol->userTypeDef, caseValueNode);
            } else if (lookupSymbolNode(caseValueNode, caseStmt->lineNum)) {
                ICG_CompareConstName(caseValueNode.value.str);
                return;
            } else {
                ErrorMessageWithList("Invalid case condition", caseStmt);
                return;
            }
            break;

        case N_LIST:
            if (isToken(caseValueNode.value.list->nodes[0], PT_PROPERTY_REF)) {
                evalResult = evaluate_node(caseValueNode);
                break;
            }
        default:
            ErrorMessageWithList("Unable to process case statement", caseStmt);
    }

    if (evalResult.hasResult) {
        ICG_CompareConst(evalResult.value);
    } else {
        ErrorMessageWithList(INVALID_CASE_ENUM_ERROR, caseStmt);
    }
}

void GC_Switch(const List *stmt, enum SymbolType destType) {
    Label *endOfSwitch = newGenericLabel(L_CODE);

    SymbolRecord *switchVarSymbol = NULL;

    if (stmt->nodes[1].type == N_LIST) {
        GC_Expression(stmt->nodes[1].value.list, ST_CHAR);
    } else if (stmt->nodes[1].type == N_STR) {
        switchVarSymbol = lookupSymbolNode(stmt->nodes[1], stmt->lineNum);
        if (switchVarSymbol != NULL) {
            ICG_LoadVar(switchVarSymbol);
        }
    } else {
        ErrorMessageWithList("Invalid expression used for switch statement", stmt);
    }

    //examineSwitchStmtCases(stmt);

    // for each case, do cmp, branch/jump to next cmp if NE
    for_range(caseStmtNum, 2, stmt->count) {
        ListNode caseStmtNode = stmt->nodes[caseStmtNum];
        if (caseStmtNode.type == N_LIST) {
            List *caseStmt = caseStmtNode.value.list;
            if (isToken(caseStmt->nodes[0], PT_CASE)) {
                Label *nextCaseLabel = newGenericLabel(L_CODE);
                IL_AddCommentToCode(buildSourceCodeLine(&caseStmt->progLine));

                // handle case condition check
                GC_HandleCase(caseStmt, switchVarSymbol);
                ICG_Branch(BNE, nextCaseLabel);

                // process case code block/statement
                GC_CodeBlock(caseStmt->nodes[2].value.list);
                ICG_Jump(endOfSwitch, "done with case");
                IL_Label(nextCaseLabel);
            } else if (isToken(caseStmt->nodes[0], PT_DEFAULT)) {
                GC_CodeBlock(caseStmt->nodes[1].value.list);
            }
        }
    }
    IL_Label(endOfSwitch);
}

void GC_For(const List *stmt, enum SymbolType destType) {
    Label *startOfLoop = newGenericLabel(L_CODE);
    Label *doneWithLoop = newGenericLabel(L_CODE);

    ListNode initStmtNode = stmt->nodes[1];
    if (initStmtNode.type != N_LIST) {
        ErrorMessageWithList("Invalid for loop initializer statement", stmt);
        return;
    }
    GC_Statement(initStmtNode.value.list);

    IL_Label(startOfLoop);

    // loop continue conditional check
    GC_HandleCondExpr(stmt->nodes[2], ST_BOOL, doneWithLoop, stmt->lineNum);

    // now process loop code
    GC_CodeBlock(stmt->nodes[4].value.list);

    // handle loop iteration
    ListNode incStmtNode = stmt->nodes[3];
    if (incStmtNode.type == N_LIST) {
        GC_Statement(incStmtNode.value.list);
    } else {
        ErrorMessageWithList("Invalid for loop next statement", stmt);
    }

    // end of for loop
    ICG_Jump(startOfLoop, "Loop back");
    IL_Label(doneWithLoop);
}

void GC_Loop(const List *stmt, enum SymbolType destType) {
    // Use variables to document usage of each statement node
    ListNode counterVarNode = stmt->nodes[1];
    ListNode counterStartValueNode = stmt->nodes[2];
    ListNode counterEndValueNode = stmt->nodes[3];
    ListNode loopCodeNode = stmt->nodes[4];

    //--- Check the loop parameters
    SymbolRecord *cntVarSym = lookupSymbolNode(counterVarNode, stmt->lineNum);
    if (cntVarSym == NULL) {
        ErrorMessageWithList("Invalid variable specified for loop statement", stmt);
        return;
    }
    if (!isConstValueNode(counterStartValueNode, stmt->lineNum)) {
        ErrorMessageWithList("Invalid value specified for initializing loop counter", stmt);
        return;
    }
    int counterStartValue = getConstValue(counterStartValueNode, stmt->lineNum);
    if (!isConstValueNode(counterEndValueNode, stmt->lineNum)) {
        ErrorMessageWithList("Invalid value specified for loop counter end value", stmt);
        return;
    }
    int counterEndValue = getConstValue(counterEndValueNode, stmt->lineNum);

    //----------------------------------------------------
    Label *startOfLoop = newGenericLabel(L_CODE);
    Label *doneWithLoop = newGenericLabel(L_CODE);

    //---  Initialize the loop counter
    ICG_LoadConst(counterStartValue, getBaseVarSize(cntVarSym));
    ICG_StoreVarSym(cntVarSym);

    // start of loop
    IL_Label(startOfLoop);
    GC_CodeBlock(loopCodeNode.value.list);

    // handle loop iteration
    ICG_OpWithVar(INC, cntVarSym, getBaseVarSize(cntVarSym));
    ICG_LoadVar(cntVarSym);
    ICG_CompareConst(counterEndValue);
    ICG_Branch(BEQ, doneWithLoop);

    // end of for loop
    ICG_Jump(startOfLoop, "Loop back");
    IL_Label(doneWithLoop);
}

void GC_While(const List *stmt, enum SymbolType destType) {
    Label *startOfLoop = newGenericLabel(L_CODE);
    Label *doneWithLoop = newGenericLabel(L_CODE);

    IL_Label(startOfLoop);

    ListNode condNode = stmt->nodes[1];
    if (condNode.type == N_LIST) {
        GC_HandleCondExpr(condNode, ST_BOOL, doneWithLoop, stmt->lineNum);
    } else if (condNode.type == N_INT) {
        if (condNode.value.num == 0) {
            ICG_Jump(doneWithLoop, "skipping loop");
        }
    } else {
        ErrorMessageWithNode("Invalid conditional expression", condNode, stmt->lineNum);
    }

    // now process code inside loop
    GC_CodeBlock(stmt->nodes[2].value.list);

    ICG_Jump(startOfLoop, "beginning of loop");
    IL_Label(doneWithLoop);
}

void GC_DoWhile(const List *stmt, enum SymbolType destType) {
    Label *startLoopLabel = newGenericLabel(L_CODE);
    IL_Label(startLoopLabel);

    GC_CodeBlock(stmt->nodes[1].value.list);

    // now process conditional logic
    ListNode condNode = stmt->nodes[2];
    if (condNode.type == N_LIST) {
        Label *doneWithLoop = newGenericLabel(L_CODE);
        IL_AddCommentToCode(buildSourceCodeLine(&condNode.value.list->progLine));
        GC_HandleCondExpr(condNode, ST_BOOL, doneWithLoop, stmt->lineNum);
        ICG_Jump(startLoopLabel, "beginning of loop");
        IL_Label(doneWithLoop);
    } else if (condNode.type == N_INT) {
        if (condNode.value.num > 0) {
            ICG_Jump(startLoopLabel, "beginning of loop");
        }
    } else {
        ErrorMessageWithNode("Invalid conditional expression", condNode, stmt->lineNum);
    }
}

//------------------------------------------------------------------------------------------------
//   Handle functions and parameters

/**
 * Handle the loading of an expression used as a function argument/parameter
 *
 * @param argNode
 * @param destType
 * @param destReg - which CPU reg should this be loaded into
 * @param lineNum
 */
void GC_HandleLoadFuncArgExpr(ListNode argNode, enum SymbolType destType, char destReg, int lineNum) {
    List *expr = argNode.value.list;
    bool isWord = ((destType == ST_INT) || (destType == ST_PTR));

    //  First evaluate the expression to catch and preprocess any simple constant expressions

    EvalResult evalResult = evaluate_expression(expr);
    if (evalResult.hasResult && !isWord) {
        // expression was evaluated and produced a numeric result, so we can just use the resulting value

        IL_SetLineComment(get_expression(expr));
        ICG_LoadRegConst(destReg, evalResult.value);

    } else {
        GC_Expression(expr, destType);
        switch (destReg) {
            case 'X': ICG_MoveAccToIndex('X'); break;
            case 'Y': ICG_MoveAccToIndex('Y'); break;
            case 'S': ICG_PushAcc(); break;
            default:break;
        }
    }
}

/**
 * Handling loading parameters to pass into function call  (function arguments)
 *
 * @param paramNode
 * @param destReg
 * @param lineNum
 */

void GC_LoadFuncArg(const SymbolRecord *curParam, ListNode argNode,
                    int lineNum) {
    SymbolRecord *varSym;
    if ((curParam->hint == VH_NONE) && (!IS_STACK_VAR(curParam))) {
        ErrorMessageWithNode("Unable to load parameter: ", argNode, lineNum);
        return;
    }

    char destReg = getDestRegFromHint(curParam->hint);
    IL_SetLineComment("loading param");

    // handle loading constants
    switch (argNode.type) {
        case N_INT:
            if (destReg == 'S') {
                GC_HandleLoad(argNode, ST_NONE, lineNum);
                ICG_PushAcc();
            } else {
                ICG_LoadRegConst(destReg, argNode.value.num);
            }
            break;
        case N_STR:
            varSym = lookupSymbolNode(argNode, lineNum); //findSymbol(mainSymbolTable, argNode.value.str);
            if (varSym == NULL) return;

            // first check to see if user is passing a struct variable.
            //   If so, we need to do something special to pass the pointer of the struct (16-bit value)
            //   TODO: fix this when 16-bit support is figured out
            if (varSym && isStructDefined(varSym)) {
                IL_AddCommentToCode("Handle struct/pointer being passed");
                ICG_LoadPointerAddr(varSym);
            } else if (destReg == 'S') {
                GC_HandleLoad(argNode, ST_NONE, lineNum);
                ICG_PushAcc();
            } else {
                ICG_LoadRegVar(varSym, destReg);
            }
            break;
        case N_LIST:
            GC_HandleLoadFuncArgExpr(argNode, ST_NONE, destReg, lineNum);
            break;
        default:
            ErrorMessageWithNode("Unsupported parameter passing",argNode, lineNum);
    }
}

void GC_FuncCall(const List *stmt, enum SymbolType destType) {
    ListNode funcNameNode = stmt->nodes[1];
    SymbolRecord* funcSym = lookupFunctionSymbolByNameNode(funcNameNode, stmt->lineNum);
    SymbolList* funcParamList = getParamSymbols(GET_LOCAL_SYMBOL_TABLE(funcSym));

    //--- Make sure function call has required parameters
    int requiredParams = (funcParamList != NULL) ? funcParamList->count : 0;
    bool hasParamList = (stmt->nodes[2].type == N_LIST);

    if ((requiredParams > 0) && !hasParamList) {
        ErrorMessageWithNode("Missing parameters in function call", funcNameNode, stmt->lineNum);
        return;
    }

    // Check for parameter list and process it
    bool requiresCleanup = false;
    if (hasParamList) {
        List *args = stmt->nodes[2].value.list;
        if ((requiredParams == args->count) && (funcParamList != NULL)) {
            requiresCleanup = true;

            for_range(argIndex, 0, funcParamList->count) {
                GC_LoadFuncArg(funcParamList->list[argIndex], args->nodes[argIndex], stmt->lineNum);
            }
        } else {
            ErrorMessageWithList("Incorrect number of parameters in function call", stmt);
        }
    }

    // Now we can cll the function
    ICG_Call(funcSym->name);

    // now clean-up stack
    if (requiresCleanup && (funcParamList != NULL)) {
        if (funcParamList->cntStackVars > 0) ICG_AdjustStack(funcParamList->cntStackVars);
    }
}

/**
 * Handle function calls within expressions
 * @param stmt
 * @param destType
 */
void GC_FuncCallExpression(const List *stmt, enum SymbolType destType) {
    GC_FuncCall(stmt, destType);
}


/**
 * Handle function calling statements (since they can easily be inlined)
 *
 * @param stmt
 * @param destType
 */
void GC_FuncCallStatement(const List *stmt, enum SymbolType destType) {
    ListNode funcNameNode = stmt->nodes[1];
    SymbolRecord* funcSym = lookupFunctionSymbolByNameNode(funcNameNode, stmt->lineNum);
    if (funcSym == NULL) return;

    if (IS_INLINED_FUNCTION(funcSym)) {
        IL_AddCommentToCode("---Start of inlined function");
        IL_AddCommentToCode(funcSym->name);
        GC_CodeBlock(funcSym->astList);
        IL_AddCommentToCode("---End of inlined function");
    } else {
        GC_FuncCall(stmt, destType);
    }
}


//===========================================================================================
//  -- Handle variables and initializers
//
//  TODO: Cleanup the 'alias' specific code a bit.  'Alias' is still a work-in-progress feature.
//

//#define DEBUG_ALIAS

void Debug_AliasEval(const List *varDef, EvalResult *evalResult) {
#ifdef DEBUG_ALIAS
    // DEBUG INFO
    printf("Evaluated alias initializer (line# %d)", varDef->lineNum);
    showNode(stdout, varDef->nodes[4], 1);
    if (evalResult != NULL) {
        printf("; resulted in %d", (*evalResult).value);
    }
    printf("\n");
#endif
}


List* getInitializer(const ListNode initDefNode) {
    List *initDef = initDefNode.value.list;
    if (initDef->nodes[0].value.parseToken == PT_INIT) {
        ListNode valueNode = initDef->nodes[1];
        if (valueNode.type == N_LIST) {
            return valueNode.value.list;
        }
    }
    return NULL;
}

void convertAliasToVariable(SymbolRecord *varSymRec, int loc) {
    setSymbolLocation(varSymRec, loc, ((loc < 256) ? SS_ZEROPAGE : SS_ABSOLUTE));
    varSymRec->kind = SK_VAR;
}

void GC_LocalVarAliasInitializer(const List *varDef, SymbolRecord *varSymRec, List *initList, enum SymbolType destType) {
    // We're aliasing something, we need to link the definition to the symbol record
    List *alias;

    List *aliasInitExpr = getInitializer(varDef->nodes[4]);
    if (aliasInitExpr != NULL) {
        alias = aliasInitExpr;

        EvalResult aliasResult = evalAsAddrLookup(alias);
        if (aliasResult.hasResult) {
            // if we can evaluate the alias... then the alias can become a local variable.
            // change symbol location and kind
            convertAliasToVariable(varSymRec, aliasResult.value);
            Debug_AliasEval(varDef, &aliasResult);
            return;
        }
    } else {
        // simple alias (only a name alias)
        alias = initList;
    }

    varSymRec->alias = alias;

    if (!TypeCheck_Alias(alias, destType)) {
        ErrorMessageWithList("Error processing alias", alias);
    } else {
        Debug_AliasEval(varDef, NULL);
    }
}


void GC_HandleGlobalAliasInitializer(const List *varDef, SymbolRecord *varSymRec) {
    // handle global alias - since it's global, it must be const
    List *aliasExpr = getInitializer(varDef->nodes[4]);
    EvalResult evalResult = evalAsAddrLookup(aliasExpr);
    if (evalResult.hasResult) {

        // change symbol location and kind
        convertAliasToVariable(varSymRec, evalResult.value);
        Debug_AliasEval(varDef, &evalResult);

    } else {
        ErrorMessageWithList("Error evaluating const alias", aliasExpr);
        varSymRec->alias = NULL;
        varSymRec->kind = 0;
    }
}

/**
 * Process variable definition initializer for a local variable
 *
 * @param varDef
 * @param destType
 */
void GC_LocalVariable(const List *varDef, enum SymbolType destType) {
    SymbolRecord *varSymRec = lookupSymbolNode(varDef->nodes[1], varDef->lineNum);
    if (varSymRec == NULL) return;

    int hasInitializer = (varDef->count >= 4) && (varDef->nodes[4].type == N_LIST);
    if (!hasInitializer) return;

    List *initList = varDef->nodes[4].value.list;

    if (IS_ALIAS(varSymRec)) {
        GC_LocalVarAliasInitializer(varDef, varSymRec, initList, destType);

    } else if (!isConst(varSymRec)) {
        // WE have an initializer for a variable, handle it!
        ListNode valueNode = initList->nodes[1];
        switch (valueNode.type) {
            case N_INT:
                ICG_LoadConst(valueNode.value.num, getBaseVarSize(varSymRec));
                break;
            case N_STR: {
                SymbolRecord *varSym = lookupSymbolNode(valueNode, initList->lineNum);
                if (varSym != NULL) ICG_LoadVar(varSym);
                /*if (getBaseVarSize(varSym) < getBaseVarSize(varSymRec)) {
                    ICG_LoadRegConst('X',0);
                }*/
            } break;
            case N_LIST:
                GC_HandleLoad(valueNode, getVarDestType(varSymRec), initList->lineNum);
                break;
            default:
                ErrorMessageWithList("Error initializing var",initList);
                return;
        }
        ICG_StoreVarSym(varSymRec);
    }
}


//-------------------------------------------------------------------
//--- Track code address
//-
// TODO:  This is temporary functionality until the "relocatable code/data" blocks issue is solved

static int codeAddr;

/**
 * Return the current code address
 * @return
 */
int ICG_GetCurrentCodeAddr() {
    return codeAddr;
}

void ICG_SetCurrentCodeAddr(int newCodeAddr) {
    codeAddr = newCodeAddr;
}

// TODO: Figure out how to do this differently to make blocks magically movable
// TODO:  This can probably be figured out later in the compile process (output layout step)


void GC_OB_AddCodeBlock(SymbolRecord *funcSym) {
    OB_AddCode(funcSym->name, funcSym->instrBlock, curBank);

    int funcAddr = ICG_GetCurrentCodeAddr();
    ICG_SetCurrentCodeAddr(funcAddr + funcSym->instrBlock->codeSize);

    setSymbolLocation(funcSym, funcAddr, SS_ROM);
}


void GC_OB_AddDataBlock(SymbolRecord *varSymRec) {
    OutputBlock *staticData = OB_AddData(varSymRec, varSymRec->astList, curBank);

    //--- track data table address / size ---
    int dataLoc = ICG_GetCurrentCodeAddr();
    ICG_SetCurrentCodeAddr(dataLoc + staticData->blockSize);

    setSymbolLocation(varSymRec, dataLoc, SS_ROM);
}


void GC_OB_AddLookupTable(SymbolRecord *varSymRec) {
    OutputBlock *staticData = OB_AddData(varSymRec, varSymRec->astList, 0);

    //--- track data table size
    int symAddr = ICG_GetCurrentCodeAddr();
    ICG_SetCurrentCodeAddr(symAddr + staticData->blockSize);

    setSymbolLocation(varSymRec, symAddr, SS_ROM);
}



//-------------------------------------------------------------------


/**
 * GC_HandleEcho
 *    - Evaluate provided list of parameters and print the results
 *      into the compiler output (NO CODE IS GENERATED).
 * @param echoDirStmt
 */
void GC_HandleEcho(const List *echoDirStmt) {
    printf("::ECHO:: ");
    for_range(paramIdx, 2, echoDirStmt->count) {
        ListNode echoNode = echoDirStmt->nodes[paramIdx];
        switch (echoNode.type) {

            case N_LIST: {
                List *expr = echoNode.value.list;
                EvalResult evalResult = evaluate_expression(expr);
                if (evalResult.hasResult) {
                    printf("%d ", evalResult.value);
                } else {
                    printf("ERR ");
                }
            } break;

            case N_STR: {
                EvalResult evalResult = evaluate_node(echoNode);
                if (evalResult.hasResult) {
                    printf("%d ", evalResult.value);
                } else {
                    printf("ERR ");
                }
            } break;

            case N_STR_LITERAL:
                printf("%s ", echoNode.value.str);
                break;

            case N_INT:
                printf("%d ", echoNode.value.num);
                break;

            default:
                ErrorMessageWithNode("Unsupported expr in #echo", echoNode, echoDirStmt->lineNum);
                break;
        }
    }
    printf("\n");
}


void GC_HandleDirective(const List *code, enum SymbolType destType) {

    // NOTE: loose casting op!
    enum CompilerDirectiveTokens directive = code->nodes[1].value.num;
    lastDirective = directive;

    switch (directive) {
        case SHOW_CYCLES:
            IL_ShowCycles();
            break;
        case HIDE_CYCLES:
            IL_HideCycles();
            break;
        case PAGE_ALIGN:
            ICG_SetCurrentCodeAddr((ICG_GetCurrentCodeAddr() + 256) & 0xff00);
            OB_MoveToNextPage();
            break;

        case INVERT:
            WarningMessage("'#invert' is deprecated", NULL, code->lineNum);
        case REVERSE:
            reverseData = true;
            break;

        case ECHO:
            GC_HandleEcho(code);
            break;
        case SET_BANK:
            curBank = code->nodes[2].value.num;
            break;
        case INLINE:
            // handle #inline directive within code block
            break;
        default:
            break;
    }
}


//-------------------------------------------------------------------
// --  Handle Statements
//-------------------------------------------------------------------

void GC_NestedCodeBlock(const List *code, enum SymbolType destType) {
    GC_CodeBlock((List *)code);
}


ParseFuncTbl stmtFunction[] = {
        {PT_ASM,        &GC_AsmBlock},
        {PT_SET,        &GC_Assignment},
        {PT_DEFINE,     &GC_LocalVariable},
        {PT_FUNC_CALL,  &GC_FuncCallStatement},
        {PT_RETURN,     &GC_Return},
        {PT_DOWHILE,    &GC_DoWhile},
        {PT_WHILE,      &GC_While},
        {PT_FOR,        &GC_For},
        {PT_LOOP,       &GC_Loop},
        {PT_STROBE,     &GC_Strobe},
        {PT_IF,         &GC_If},
        {PT_SWITCH,     &GC_Switch},
        {PT_INC,        &GC_IncStmt},
        {PT_DEC,        &GC_DecStmt},
        {PT_DIRECTIVE,  &GC_HandleDirective},
        {PT_CODE,       &GC_NestedCodeBlock},
};
const int stmtFunctionSize = sizeof(stmtFunction) / sizeof(ParseFuncTbl);

void GC_Statement(List *stmt) {
    int funcIndex;
    ListNode opNode = stmt->nodes[0];
    if (opNode.type == N_TOKEN) {

        // using statement token, lookup and call appropriate code generator function
        funcIndex = findFunc(stmtFunction, stmtFunctionSize, opNode.value.parseToken);
        if (funcIndex >= 0) {
            stmtFunction[funcIndex].parseFunc(stmt, ST_NONE);
        }
    } else {
        ErrorMessageWithList("Error in statement: ", stmt);
    }
}


//-----------------------------------------------------------------------
//  High level structure items

static const char* INIT_VALUE_ERROR_MSG = "Initializer value cannot be evaluated";

/**
 * Preprocess an initializer expression by running it thru the evaluator
 * @param initExpr
 * @param lineNum
 * @return
 */
ListNode PreProcess_Node(ListNode initExpr, int lineNum) {
    int value = 0;
    ListNode resultNode;
    EvalResult result = evaluate_node(initExpr);
    if (result.hasResult) {
        value = result.value;
    } else {
        ErrorMessageWithNode(INIT_VALUE_ERROR_MSG, initExpr, lineNum);
    }
    resultNode = createIntNode(value & 0xffff);
    return resultNode;
}

/**
 * Preprocess Initializer Data List in preparation for code generation
 *
 * -- Runs the Expression evaluator and replaces constant vars with values.
 *
 * @param initList
 */
void Preprocess_InitListData(const List* varDef, List *initList) {
    int index=1;
    if (initList == NULL) {
        ErrorMessageWithList("Initializer invalid", varDef);
        return;
    }
    while (index < initList->count) {
        ListNode initExpr = initList->nodes[index];
        ListNode processedNode = PreProcess_Node(initExpr, initList->lineNum);
        if (processedNode.type != N_EMPTY) {
            initList->nodes[index] = processedNode;
        }
        index++;
    }
}

ListNode Preprocess_InitData(const List *varDef, ListNode nestedNode) {
    ListNode processedNode;
    processedNode.type = N_EMPTY;

    switch (nestedNode.type) {
        case N_LIST: {
            List *nestedValueList = nestedNode.value.list;
            if (isToken(nestedValueList->nodes[0], PT_LIST)) {
                Preprocess_InitListData(varDef, nestedValueList);
            } else {
                processedNode = PreProcess_Node(nestedNode, nestedValueList->lineNum);
            }
        } break;
        case N_STR: {
            int value;
            SymbolRecord *constSym = lookupSymbolNode(nestedNode, varDef->lineNum);
            if (constSym && constSym->hasValue) {
                value = constSym->constValue;
            } else {
                value = 0;
            }
            processedNode = createIntNode(value & 0xffff);
        } break;
        default: break;
    }
    return processedNode;
}


List *GC_ProcessInitializerList(const List *varDef, List *initValueList) {
    if (initValueList->hasNestedList &&
            isToken(initValueList->nodes[0], PT_LIST)) {
        // need to loop over lists
        for_range (nestedCnt, 1, initValueList->count) {
            ListNode processedNode = Preprocess_InitData(varDef, initValueList->nodes[nestedCnt]);
            if (processedNode.type != N_EMPTY) {
                initValueList->nodes[nestedCnt] = processedNode;
            }
        }
    } else {
        // need to evaluate each list value first
        Preprocess_InitListData(varDef, initValueList);
    }

    if (reverseData) {
        reverseList(initValueList);
    }

    // need to do this because the pointer has changed at this point...
    List *valueList = varDef->nodes[4].value.list->nodes[1].value.list;
    return valueList;
}

/**
 * Generate Code for global variables that have initializers.
 * @param varDef
 */
void GC_Variable(const List *varDef) {
    char *varName = varDef->nodes[1].value.str;
    SymbolRecord *varSymRec = findSymbol(mainSymbolTable, varName);

    // does this variable definition have a list of initial values?
    int hasInitializer = (varDef->count >= 4) && (varDef->nodes[4].type == N_LIST);

    //---  Handle index generation via #directive
    if (lastDirective == USE_QUICK_INDEX_TABLE) {
        unsigned char multiplier = getBaseVarSize(varSymRec);

        // only generate a multiplication lookup table if the multiplier is greater than 2 (not a primitive var)
        if (multiplier > 2) {
            SymbolRecord *lookupTable = ICG_Mul_AddLookupTable(multiplier);
            GC_OB_AddLookupTable(lookupTable);

        } else {
            printf("Warning: Cannot generate quick index table for %s\n", varName);
        }
        lastDirective = 0;
    }

    if (!hasInitializer) return;

    if (IS_ALIAS(varSymRec)) {

        GC_HandleGlobalAliasInitializer(varDef, varSymRec);

    } else if (isArray(varSymRec)) {
        if (!isConst(varSymRec)) {
            ErrorMessageWithList("Non-const array cannot be initialized with data", varDef);
            return;
        }

        List *initValueList = getInitializer(varDef->nodes[4]);
        if (initValueList == NULL) return;

        if (varSymRec->userTypeDef != NULL && isEnum(varSymRec->userTypeDef)) {
            setEvalLocalSymbolTable(varSymRec->userTypeDef->symbolTbl);
            printf("Using local symbol table for %s: %s\n", varName, varSymRec->userTypeDef->name);
        }

        // --- Process the initializer list! ---
        List *valueNode = GC_ProcessInitializerList(varDef, initValueList);
        reverseData = false;
        varSymRec->astList = valueNode;
        GC_OB_AddDataBlock(varSymRec);

    }
}

void GC_StatementList(const List *code) {
    for_range(stmtNum, 1, code->count) {
        ListNode stmtNode = code->nodes[stmtNum];
        if (stmtNode.type == N_LIST) {
            List *stmt = stmtNode.value.list;
            IL_AddCommentToCode(buildSourceCodeLine(&stmt->progLine));
            GC_Statement(stmt);
        } else if (isToken(stmtNode, PT_BREAK)) {
            // Need to add code to exit block for break statement
        } else {
            printf("Unknown command: %s\n", stmtNode.value.str);
        }
    }
}

void GC_CodeBlock(List *code) {
    if (code->count < 1) return;     // Exit if empty function

    if (code->nodes[0].value.parseToken == PT_ASM) {
        GC_AsmBlock(code, ST_NONE);
    } else {
        GC_StatementList(code);
    }
}

//==================================================================================
//------  Handle functions

/*** Preload any parameters into appropriate registers */
void GC_PreloadParams(SymbolList *params) {
    for_range(paramCnt, 0, params->count) {
        SymbolRecord *curParam = params->list[paramCnt];
        if (curParam->hint != VH_NONE) IL_Preload(curParam);
    }
}

void GC_ProcessFunction(SymbolRecord *funcSym, List *code) {
    char *funcName = funcSym->name;
    Label *funcLabel = newLabel(funcName, L_CODE);

    // start building function using provided label
    ICG_StartOfFunction(funcLabel, funcSym);

    // load in local symbol table for function
    curFuncSymbolTable = GET_LOCAL_SYMBOL_TABLE(funcSym);
    setEvalLocalSymbolTable(curFuncSymbolTable);

    // need to let the code generator/instruction generator
    //  know about parameters that have been preloaded
    //   into registers
    SymbolList* funcParamList = getParamSymbols(curFuncSymbolTable);
    if (funcParamList != NULL) {
        GC_PreloadParams(funcParamList);
    }

    GC_CodeBlock(code);
    funcSym->instrBlock = ICG_EndOfFunction(funcLabel);
    funcSym->astList = code;

    setEvalLocalSymbolTable(NULL);
}

void GC_Function(const List *function, int codeNodeIndex) {
    bool hasCode, isFuncUsed=false;
    char *funcName = function->nodes[1].value.str;

    if (function->count < codeNodeIndex) {
        ErrorMessage("Error processing function", funcName, function->lineNum);
        return;
    }

    // check if function has been flagged to be inlined wherever it's used
    bool isInlineFunction = (lastDirective == INLINE);
    if (isInlineFunction) {
        printf("Inline function %s\n", funcName);
        lastDirective = 0;
    }

    ListNode codeNode = function->nodes[codeNodeIndex];
    hasCode = (codeNode.type == N_LIST);

    // only process function code, if it exists, and the function is used
    if (hasCode) {
        SymbolRecord *funcSym = findSymbol(mainSymbolTable, funcName);
        isFuncUsed = isMainFunction(funcSym) || IS_FUNC_USED(funcSym);
        if (isFuncUsed) {
            if (isInlineFunction) {
                // save a pointer to the part of the AST containing the code list and mark as INLINE.
                funcSym->astList = codeNode.value.list;
                funcSym->flags |= MF_INLINE;
            } else {
                GC_ProcessFunction(funcSym, codeNode.value.list);
                GC_OB_AddCodeBlock(funcSym);
            }
        } else {

            // process function -> just so we can get its size / show its code
            GC_ProcessFunction(funcSym, codeNode.value.list);
        }
    }

    if (compilerOptions.reportFunctionProcessing) {
        if (hasCode && isFuncUsed) {
            printf("Processed function: %s\n", funcName);
        } else if (hasCode) {
            printf("Skipped unused function %s\n", funcName);
        } else {
            printf("Processed function definition: %s\n", funcName);
        }
    }
}

//--------------------------------------------------------------
//  Walk the program

void GC_ProcessProgramNode(ListNode opNode, const List *statement) {
    if (opNode.type == N_TOKEN) {
        // Only need to process functions since variables were already processed in the symbol generator module
        switch (opNode.value.parseToken) {
            case PT_DEFUN:    GC_Function(statement, 5); break;
            case PT_FUNCTION: GC_Function(statement, 3); break;
            case PT_DEFINE:   GC_Variable(statement); break;
            case PT_STRUCT: break;
            case PT_UNION:  break;
            case PT_ENUM:   break;
            case PT_DIRECTIVE:
                GC_HandleDirective(statement, 0);
                break;
            default:
                ErrorMessageWithList("Program code found outside code block", statement);
        }
    }
    curFuncSymbolTable = NULL;
}

void WalkProgram(List *program, ProcessNodeFunc procNode) {
    for_range (stmtNum, 1, program->count) {
        ListNode stmt = program->nodes[stmtNum];
        if (stmt.type == N_LIST) {
            List *statement = stmt.value.list;
            ListNode opNode = statement->nodes[0];
            procNode(opNode, statement);
        }
    }
}

void GC_ProcessProgram(ListNode node) {

    // make sure to reset error count
    GC_ErrorCount = 0;
    if (node.type != N_LIST) return;

    List *program = node.value.list;
    if (program->nodes[0].value.parseToken != PT_PROGRAM) return;

    // generate all the code for the program
    WalkProgram(program, &GC_ProcessProgramNode);
}

//-------------------------------------------------------------------
//===


void initCodeGenerator(SymbolTable *symbolTable, enum Machines machines) {
    mainSymbolTable = symbolTable;
    ICG_Mul_InitLookupTables(symbolTable);
    IL_Init();
    ICG_SetCurrentCodeAddr(getMachineStartAddr(machines));
}

void generate_code(char *name, ListNode node) {
    if (compilerOptions.showGeneralInfo) printf("Generating Code for %s\n", name);

    curBank = 0;
    curFuncSymbolTable = NULL;
    reverseData = false;

    initEvaluator(mainSymbolTable);

    FE_initDebugger();

    // process module -> just needs to run code generator to build output blocks
    GC_ProcessProgram(node);

    FE_killDebugger();
}