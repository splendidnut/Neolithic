//
//  Generate Code by walking the abstract syntax tree
//
// Created by admin on 3/28/2020.
//
//  TODO:
//      Cleanup code for accessing line number information
//

#include <stdio.h>
#include <stdlib.h>

#include "common/common.h"
#include "data/symbols.h"
#include "cpu_arch/instrs.h"
#include "cpu_arch/instrs_math.h"
#include "eval_expr.h"
#include "output/write_output.h"
#include "output/output_block.h"
#include "common/tree_walker.h"
#include "gen_common.h"
#include "parser/parse_directives.h"
#include "flatten_tree.h"
#include "gen_asmcode.h"

//-------------------------------------------
//  Variables used in code generation

static bool reverseData;


//--------------------------------------------------------
//--- Forward References

void GC_Expression(const List *expr, enum SymbolType destType);
void GC_Statement(List *stmt);
void GC_CodeBlock(List *code);
void GC_FuncCall(const List *stmt, enum SymbolType destType);

//-----------------------------------------------------------------------------
//  Some utility functions

// TODO: This method could use some more cleanup.
SymbolRecord* getArraySymbol(const List *expr, ListNode arrayNode) {
    SymbolRecord *arraySymbol = NULL;
    switch (arrayNode.type) {
        case N_LIST:
            // if we attempt to get an array symbol and find a list,
            //   then we actually have an address operator being applied to the array.
            arraySymbol = 0;
            break;
        case N_STR:
            arraySymbol = lookupSymbolNode(arrayNode, expr->lineNum);
            break;
        default:
            ErrorMessageWithNode("Invalid array lookup", arrayNode, expr->lineNum);
            return NULL;
    }

    if ((arraySymbol != NULL) && (!isArray(arraySymbol) && !isPointer(arraySymbol))) {
        ErrorMessageWithNode("Not an array or pointer", arrayNode, expr->lineNum);
        arraySymbol = NULL;
    }
    return arraySymbol;
}


bool isParam(const char *varName) {
    SymbolRecord *symRec = NULL;
    if (curFuncParamTable != NULL)
        symRec = findSymbol(curFuncParamTable, varName);
    return (symRec != NULL);
}

//-------------------------------------------------------
//   Simple Code Generation

void GC_LoadParamVar(ListNode loadNode, const char *varName, int lineNum) {
    SymbolRecord *varRec = findSymbol(curFuncParamTable, varName);
    switch (varRec->hint) {
        case VH_A_REG: break;   // already in A
        case VH_X_REG: ICG_MoveIndexToAcc('X'); break;
        case VH_Y_REG: ICG_MoveIndexToAcc('Y'); break;
        default: {
            // must be a stack var
            if (varRec->isStack) {
                ICG_LoadFromStack(varRec->location);
            } else {
                ErrorMessageWithNode("Inaccessible parameter", loadNode, lineNum);
            }
        }
    }
}


void GC_LoadVar(ListNode loadNode, int lineNum) {
    char *varName = loadNode.value.str;
    if (isParam(varName)) {
        GC_LoadParamVar(loadNode, varName, lineNum);
    } else {
        SymbolRecord *varRec = lookupSymbolNode(loadNode, lineNum);
        if (varRec != NULL) ICG_LoadVar(varRec);
    }
}

void GC_LoadPrimitive(ListNode loadNode, enum SymbolType destType, int lineNum) {
    int destSize = (destType == ST_INT) ? 2 : 1;
    if (loadNode.type == N_STR) {
        GC_LoadVar(loadNode, lineNum);
    } else if (loadNode.type == N_INT) {
        ICG_LoadConst(loadNode.value.num, destSize);
    } else {
        ErrorMessageWithList("Error loading primitive", wrapNode(loadNode));
    }
}

void GC_HandleLoad(ListNode loadNode, enum SymbolType destType, int lineNum) {
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

void GC_LoadFromArray(const SymbolRecord *srcSym) {
    if (isPointer(srcSym)) {
        ICG_LoadIndirect(srcSym, 0);
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
                GC_LoadFromArray(arraySymbol);
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

/**
 * Lookup offset for use in INC, DEC, storage expr, or ASM instructions
 *
 * Used for Array offsets used on storage variables
 *
 * @param expr
 * @return
 */
int GC_LookupArrayOfs(const List *expr) {
    SymbolRecord* arraySymbol = getArraySymbol(expr, expr->nodes[1]);

    if (arraySymbol == NULL) return -1;

    ListNode indexNode = expr->nodes[2];

    int multiplier = getBaseVarSize(arraySymbol);
    int index = 0;
    switch (indexNode.type) {
        case N_INT:     // numeric constant used as index
            index = indexNode.value.num;
            break;
        case N_STR: {   // variable used as index
            SymbolRecord *varSym = lookupSymbolNode(indexNode, expr->lineNum);
            if (varSym) {
                if (isConst(varSym)) {
                    index = varSym->constValue;
                } else {
                    ICG_LoadIndexVar(varSym, getBaseVarSize(varSym));
                    return -1000;
                }
            }
        } break;
        default:
            ErrorMessageWithList("Invalid array lookup", expr);
    }
    //printf(" array: %s @ %d[%d]\n", arraySymbol->name, arraySymbol->location, index);
    return arraySymbol->location + (index * multiplier);
}


//-------------------------------------------------------------------------------------------
//  Functions to handle Struct property references
//---


void addStructRefComment(const char *prefixComment, const char *structName, const char *propName) {
    char *propRefStr = malloc(40);
    sprintf(propRefStr, "%s: %s.%s", prefixComment, structName, propName);
    IL_AddCommentToCode(propRefStr);
}


// TODO:  Try to eliminate the need for this function.
//          It mucks up the ASM output... uses address instead of symbol name.
//          So the addStructRefComment function is being used to let the programmer know what is going on.

int GC_GetPropertyRefOfs(const List *expr) {
    if (expr->nodes[2].type != N_STR) {
        ErrorMessageWithNode("Invalid property - not an identifier", expr->nodes[2], expr->lineNum);
        return -1;
    }

    char *propName = expr->nodes[2].value.str;

    // setup line comment for property references
    // TODO: Maybe add a property reference labeling system for easier to read ASM code?

    if (expr->nodes[1].type == N_STR) {
        char *structName = expr->nodes[1].value.str;
        addStructRefComment("GC_GetPropertyRefOfs", structName, propName);
    }

    SymbolRecord *structSymbol = lookupSymbolNode(expr->nodes[1], expr->lineNum);

    //--- DEBUG Code
    /*
    if (structSymbol->kind == SK_ALIAS) {
        ErrorMessageWithList("Need to handle alias", expr);
        ErrorMessageWithList(" with the following:", structSymbol->alias);
    }
    //*/

    int ofs = -1;
    if (isStructDefined(structSymbol)) {
        ofs = structSymbol->location;
        SymbolRecord *propertySymbol = findSymbol(getStructSymbolSet(structSymbol), propName);

        if (propertySymbol != NULL) {
            ofs = structSymbol->location + propertySymbol->location;
        } else {
            ErrorMessage("Missing property: ", propName, expr->lineNum);
        }
    }
    return ofs;
}


// TODO: Improve this function to be able to handle more cases.
//       Need to handle referencing struct pointer case.
//       Need to produce better ASM source code.

void GC_LoadAlias(const List *expr, SymbolRecord *structSymbol) {
    List *alias = structSymbol->alias;

    // exit early if alias is already loaded
    if (ICG_IsCurrentTag('Y', structSymbol)) return;

    if (isToken(alias->nodes[0], PT_LOOKUP)) {

        SymbolRecord *arraySymbol = lookupSymbolNode(alias->nodes[1], alias->lineNum);
        SymbolRecord *indexSymbol = lookupSymbolNode(alias->nodes[2], alias->lineNum);
        //ListNode indexNode = alias->nodes[2];

        int multiplier = calcVarSize(structSymbol);
        ICG_MultiplyWithConst(indexSymbol, multiplier & 0x7f);
        ICG_MoveAccToIndex('Y');

        // Make sure instruction generator knows the use, so we don't have to keep reloading
        ICG_Tag('Y', structSymbol);

    } else {
        ErrorMessageWithList("-Need to handle alias", expr);
        ErrorMessageWithList("- with the following:", structSymbol->alias);
    }
}

SymbolRecord *GC_GetAliasBase(const List *expr, SymbolRecord *structSymbol) {
    List *alias = structSymbol->alias;
    if (isToken(alias->nodes[0], PT_LOOKUP)) {
        return lookupSymbolNode(alias->nodes[1], alias->lineNum);
    } else {
        return NULL;
    }
}

void GC_LoadPropertyRef(const List *expr, enum SymbolType destType) {
    if ((expr->nodes[1].type != N_STR) || (expr->nodes[2].type != N_STR)) {
        ErrorMessageWithList("GC_LoadPropertyRef cannot handle",expr);
        return;
    }

    SymbolRecord *structSymbol = lookupSymbolNode(expr->nodes[1], expr->lineNum);
    if (!structSymbol) return;      /// EXIT if invalid structure

    char *propName = expr->nodes[2].value.str;
    SymbolRecord *propertySymbol = findSymbol(getStructSymbolSet(structSymbol), propName);
    if (!propertySymbol) return;      /// EXIT if invalid structure property

    // Now we can get down to business!

    if (isStructDefined(structSymbol) && IS_PARAM_VAR(structSymbol)) {

        IL_AddCommentToCode("using GC_LoadPropertyRef -- is param");

        ICG_LoadRegConst('Y', propertySymbol->location);
        ICG_LoadIndirect(structSymbol, 0);

    } else if (structSymbol->kind == SK_ALIAS) {
        GC_LoadAlias(expr, structSymbol);
        SymbolRecord *baseSymbol = GC_GetAliasBase(expr, structSymbol);
        ICG_LoadIndexedWithOffset(baseSymbol, propertySymbol->location);
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

bool isSimplePropertyRef(ListNode arg2) {
    return (arg2.type == N_LIST)
               && isToken(arg2.value.list->nodes[0], PT_PROPERTY_REF)
               && (arg2.value.list->hasNestedList == false);
}

void GC_OP(const List *expr, enum MnemonicCode mne, enum SymbolType destType, enum MnemonicCode preOp) {
    int destSize = (destType == ST_INT || destType == ST_PTR) ? 2 : 1;

    ListNode arg1 = expr->nodes[1];
    ListNode arg2 = expr->nodes[2];

    // if second arg is a list and first is not,
    //   swap order of arguments if order of args for the given operation doesn't matter
    bool isInterchangeableOp = ((mne == AND) || (mne == ORA) || (mne == EOR));
    if (isInterchangeableOp
         && (arg2.type == N_LIST)
         && ((arg1.type == N_INT) || (arg1.type == N_STR))) {
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
        if (structSymbol && (structSymbol->kind == SK_ALIAS)) {
            SymbolRecord *propertySymbol = findSymbol(getStructSymbolSet(structSymbol), propertyRef->nodes[2].value.str);
            if (!propertySymbol) return;      /// EXIT if invalid structure property

            ICG_PreOp(preOp);

            GC_LoadAlias(propertyRef, structSymbol);
            SymbolRecord *baseSymbol = GC_GetAliasBase(expr, structSymbol);
            ICG_OpIndexedWithOffset(mne, baseSymbol, propertySymbol->location);
        } else {
            int ofs = GC_GetPropertyRefOfs(arg2.value.list);
            ICG_PreOp(preOp);
            ICG_OpWithAddr(mne, ofs);
        }
    } else if (arg2.type == N_LIST) {
        ICG_PushAcc();
        GC_Expression(arg2.value.list, destType);
        ICG_PreOp(preOp);
        ICG_OpWithStack(mne);
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

void GC_IncStmt(const List *stmt, enum SymbolType destType) {
    if (stmt->nodes[1].type == N_LIST) {
        int ofs;
        List *expr = stmt->nodes[1].value.list;
        switch (expr->nodes[0].value.parseToken) {
            case PT_LOOKUP:
                ofs = GC_LookupArrayOfs(expr);
                ICG_IncUsingAddr(ofs, 1);
                break;
            case PT_PROPERTY_REF:
                ofs = GC_GetPropertyRefOfs(expr);
                ICG_IncUsingAddr(ofs, 1);
                break;
            default:
                ErrorMessageWithList("Invalid increment statement", stmt);
        }
    } else if (stmt->nodes[1].type == N_STR) {
        GC_Inc(stmt, destType);
    }
}

void GC_DecStmt(const List *stmt, enum SymbolType destType) {
    if (stmt->nodes[1].type == N_LIST) {
        int ofs;
        List *expr = stmt->nodes[1].value.list;
        switch (expr->nodes[0].value.parseToken) {
            case PT_LOOKUP:
                ofs = GC_LookupArrayOfs(expr);
                ICG_DecUsingAddr(ofs, 1);
                break;
            case PT_PROPERTY_REF:
                ofs = GC_GetPropertyRefOfs(expr);
                ICG_DecUsingAddr(ofs, 1);
                break;
            default:
                ErrorMessageWithList("Invalid decrement statement", stmt);
        }
    } else if (stmt->nodes[1].type == N_STR) {
        GC_Dec(stmt, destType);
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

void GC_MultiplyOp(const List *expr, enum SymbolType destType) {
    SymbolRecord *varSym = lookupSymbolNode(expr->nodes[1], expr->lineNum);

    if (varSym == NULL) return;

    if (expr->nodes[2].type == N_INT) {
        ICG_MultiplyWithConst(varSym, expr->nodes[2].value.num);
    } else if (expr->nodes[2].type == N_STR) {
        SymbolRecord *varSym2 = lookupSymbolNode(expr->nodes[2], expr->lineNum);
        ICG_MultiplyWithVar(varSym, varSym2);
    } else {
        ErrorMessageWithNode("Multiply op not supported", expr->nodes[2], expr->lineNum);
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
        {PT_FUNC_CALL,      &GC_FuncCall},
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
    if (structSym == NULL) return;

    if (isStructDefined(structSym)) {
        SymbolRecord *propertySym = findSymbol(getStructSymbolSet(structSym), propName);

        if (propertySym != NULL) {
            if (structSym->kind == SK_ALIAS) {

                GC_LoadAlias(expr, structSym);
                SymbolRecord *baseSymbol = GC_GetAliasBase(expr, structSym);
                ICG_StoreIndexedWithOffset(baseSymbol, propertySym->location);

            } else {

                int ofs = propertySym->location;
                int destSize = getBaseVarSize(propertySym);
                ICG_StoreVarOffset(structSym, ofs, destSize);
            }
        }
    }
}

/*** Process Storage Expression - (Left-side of assignment) */
void GC_ExpressionForStore(const List *expr, enum SymbolType destType) {
    ListNode opNode = expr->nodes[0];
    if (opNode.type == N_TOKEN) {
        switch (opNode.value.parseToken) {
            case PT_PROPERTY_REF:
                GC_StoreToStructProperty(expr);
                break;
            case PT_LOOKUP: {
                SymbolRecord *varSymRec = lookupSymbolNode(expr->nodes[1], expr->lineNum);
                IL_SetLineComment(varSymRec->name);

                int ofs = GC_LookupArrayOfs(expr);
                if (ofs == -1000) { // means that index has already been loaded
                    ICG_StoreVarIndexed(varSymRec);
                } else {
                    ICG_StoreVarOffset(varSymRec, ofs - varSymRec->location, getBaseVarSize(varSymRec));
                }
            } break;
            case PT_INC:  GC_Inc(expr, ST_NONE);   break;
            case PT_DEC:  GC_Dec(expr, ST_NONE);   break;
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

enum SymbolType getAsgnDestType(const List *stmt, ListNode storeNode, enum SymbolType destType) {
    SymbolRecord *destVar = NULL;
    switch (storeNode.type) {
        case N_STR:
            destVar = lookupSymbolNode(storeNode, stmt->lineNum);
            break;
        case N_LIST: {
            List *storeExpr = storeNode.value.list;
            // probably a property ref or an array lookup
            if (isToken(storeExpr->nodes[0], PT_LOOKUP)) {

                destVar = lookupSymbolNode(storeExpr->nodes[1], stmt->lineNum);

            } else if (isToken(storeExpr->nodes[0], PT_PROPERTY_REF)) {

                char *propName = storeExpr->nodes[2].value.str;
                SymbolRecord *structVar = lookupSymbolNode(storeExpr->nodes[1], stmt->lineNum);
                if (structVar != NULL) {
                    SymbolRecord *propertySym = findSymbol(getStructSymbolSet(structVar), propName);
                    destVar = propertySym;
                    if (propertySym == NULL) {
                        ErrorMessageWithList("Property not found", storeExpr);
                    }
                }
            } else {
                printf("UNKNOWN dest type expression");
            }
        } break;
    }
    return (destVar != NULL) ? getVarDestType(destVar) : ST_ERROR;
}


void GC_Assignment(const List *stmt, enum SymbolType destType) {
    ListNode loadNode = stmt->nodes[2];
    ListNode storeNode = stmt->nodes[1];

    // figure out destination type
    destType = getAsgnDestType(stmt, storeNode, destType);

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
            ErrorMessageWithList("Unsupported compare op: ", wrapNode(arg));
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
void GC_Switch(const List *stmt, enum SymbolType destType) {
    Label *endOfSwitch = newGenericLabel(L_CODE);

    if (stmt->nodes[1].type == N_LIST) {
        GC_Expression(stmt->nodes[1].value.list, ST_CHAR);
    } else if (stmt->nodes[1].type == N_STR) {
        SymbolRecord *varSym = lookupSymbolNode(stmt->nodes[1], stmt->lineNum);
        if (varSym != NULL) {
            ICG_LoadVar(varSym);
        }
    } else {
        ErrorMessageWithList("Invalid expression used for switch statement", stmt);
    }

    //examineSwitchStmtCases(stmt);

    // for each case, do cmp, branch/jump to next cmp if NE
    for (int caseStmtNum = 2;  caseStmtNum < stmt->count;  caseStmtNum++) {
        ListNode caseStmtNode = stmt->nodes[caseStmtNum];
        if (caseStmtNode.type == N_LIST) {
            List *caseStmt = caseStmtNode.value.list;
            if (caseStmt->nodes[0].value.parseToken == PT_CASE) {
                Label *nextCaseLabel = newGenericLabel(L_CODE);

                // handle case condition check
                ListNode caseValueNode = caseStmt->nodes[1];
                switch (caseValueNode.type) {
                    case N_INT: ICG_CompareConst(caseValueNode.value.num); break;
                    case N_STR: ICG_CompareConstName(caseValueNode.value.str); break;
                }
                ICG_Branch(BNE, nextCaseLabel);

                // process case code block/statement
                GC_CodeBlock(caseStmt->nodes[2].value.list);
                ICG_Jump(endOfSwitch, "done with case");
                IL_Label(nextCaseLabel);
            } else if (caseStmt->nodes[0].value.parseToken == PT_DEFAULT) {
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
        ErrorMessageWithList("Invalid for loop next statment", stmt);
    }

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

/**
 * Handling loading parameters to pass into function call
 *
 * @param paramNode
 * @param destReg
 * @param lineNum
 */
void GC_HandleParamLoad(const ListNode paramNode, const char destReg, int lineNum) {
    IL_SetLineComment("loading param");

    // handle loading constants
    if (paramNode.type == N_INT) {
        if (destReg == 'S') {
            GC_HandleLoad(paramNode, ST_NONE, lineNum);
            ICG_PushAcc();
        } else {
            ICG_LoadRegConst(destReg, paramNode.value.num);
        }
    } else {

        // first check to see if user is passing a struct variable.
        //   If so, we need to do something special to pass the pointer of the struct (16-bit value)
        //   TODO: fix this when 16-bit support is figured out
        if (paramNode.type == N_STR) {
            SymbolRecord *varSym = findSymbol(mainSymbolTable, paramNode.value.str);
            if (varSym && isStructDefined(varSym)) {
                IL_AddCommentToCode("Handle struct/pointer being passed");
                ICG_LoadPointerAddr(varSym);
                return;
            }
        }
        GC_HandleLoad(paramNode, ST_NONE, lineNum);
        switch (destReg) {
            case 'X': ICG_MoveAccToIndex('X'); break;
            case 'Y': ICG_MoveAccToIndex('Y'); break;
            case 'S': ICG_PushAcc(); break;
        }
    }
}

void GC_FuncCallCleanup(SymbolRecord *firstParam) {
    int stackAdj = 0;
    while (firstParam != NULL) {
        if (firstParam->isStack) stackAdj++;
        firstParam = firstParam->next;
    }
    if (stackAdj>0) ICG_AdjustStack(stackAdj);
}

void GC_FuncCall(const List *stmt, enum SymbolType destType) {
    ListNode funcNameNode = stmt->nodes[1];
    char *funcName = stmt->nodes[1].value.str;
    SymbolRecord *funcSym = findSymbol(mainSymbolTable, funcName);

    if (funcSym == NULL) {
        ErrorMessageWithNode("Function not defined", funcNameNode, stmt->lineNum);
        return;
    }

    SymbolTable *funcParams;
    SymbolRecord *curParam;

    int requiredParams = funcSym->funcExt->cntParams;
    bool requiresCleanup = false;

    // Check for parameter list and process it
    if (stmt->nodes[2].type == N_LIST) {
        List *args = stmt->nodes[2].value.list;
        int lineNum = stmt->lineNum;
        if (requiredParams == args->count) {

            requiresCleanup = true;

            // now we can load the parameters
            funcParams = funcSym->funcExt->paramSymbolSet;
            curParam = funcParams->firstSymbol;
            int argIndex = 0;
            while (curParam != NULL) {
                // If param has a hint, check to see if it's available;
                switch (curParam->hint) {
                    case VH_A_REG: GC_HandleParamLoad(args->nodes[argIndex], 'A', lineNum); break;
                    case VH_X_REG: GC_HandleParamLoad(args->nodes[argIndex], 'X', lineNum); break;
                    case VH_Y_REG: GC_HandleParamLoad(args->nodes[argIndex], 'Y', lineNum); break;

                    // otherwise, attempt to put param on stack
                    default: {
                        if (curParam->isStack) {
                            GC_HandleParamLoad(args->nodes[argIndex], 'S', lineNum);
                        } else {
                            ErrorMessageWithList("Unable to load parameter: ", args);
                        }
                    }
                }
                argIndex++;
                curParam = curParam->next;
            }
        } else {
            ErrorMessageWithList("Incorrect number of parameters in function call", stmt);
        }
    } else if (requiredParams > 0) {
        ErrorMessageWithNode("Missing parameters in function call", funcNameNode, stmt->lineNum);
    }
    ICG_Call(funcName);

    // now clean-up stack
    if (requiresCleanup && (funcParams->firstSymbol != NULL)) {
        GC_FuncCallCleanup(funcParams->firstSymbol);
    }
}

void GC_LocalVariable(const List *varDef, enum SymbolType destType) {
    SymbolRecord *varSymRec = lookupSymbolNode(varDef->nodes[1], varDef->lineNum);
    if (varSymRec == NULL) return;

    varSymRec->isLocal = true;

    int hasInitializer = (varDef->count >= 4) && (varDef->nodes[4].type == N_LIST);
    if (!hasInitializer) return;

    List *initList = varDef->nodes[4].value.list;

    if (varSymRec->kind == SK_ALIAS) {
        // We're aliasing something, we need to link the definition to the symbol record

        varSymRec->alias = (List *)(initList->nodes[1].value.list);

    } else if (!isConst(varSymRec)) {
        // WE have an initializer for a variable, handle it!

        // TODO: Not sure if this check is even required
        if (initList->nodes[0].value.parseToken != PT_INIT) {
            ErrorMessageWithNode("Parser error", varDef->nodes[4], varDef->lineNum);
            return;
        }

        ListNode valueNode = initList->nodes[1];
        switch (valueNode.type) {
            case N_INT:
                ICG_LoadConst(valueNode.value.num, 0);
                break;
            case N_STR: {
                SymbolRecord *varSym = lookupSymbolNode(valueNode, initList->lineNum);
                if (varSym != NULL) ICG_LoadVar(varSym);
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


void GC_HandleDirective(const List *code, enum SymbolType destType) {

    // NOTE: loose casting op!
    enum CompilerDirectiveTokens directive = code->nodes[1].value.num;

    switch (directive) {
        case SHOW_CYCLES:
            printf("Cycle counts on\n");
            IL_ShowCycles();
            break;
        case HIDE_CYCLES:
            printf("Cycle counts off\n");
            IL_HideCycles();
            break;
        case PAGE_ALIGN:
            IL_MoveToNextPage();
            OB_MoveToNextPage();
            break;
        case INVERT:
            reverseData = true;
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
        {PT_FUNC_CALL,  &GC_FuncCall},
        {PT_RETURN,     &GC_Return},
        {PT_DOWHILE,    &GC_DoWhile},
        {PT_WHILE,      &GC_While},
        {PT_FOR,        &GC_For},
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

ListNode PreProcess_Node(ListNode initExpr, int lineNum) {
    int value = 0;
    ListNode resultNode;
    switch (initExpr.type) {

        case N_LIST: {                //---  Attempt to evaluate the expression
            EvalResult result = evaluate_expression(initExpr.value.list);
            if (result.hasResult) {
                value = result.value;
            } else {
                ErrorMessageWithList(INIT_VALUE_ERROR_MSG, initExpr.value.list);
            }
        } break;

        case  N_STR: {
            SymbolRecord *symbolRecord = lookupSymbolNode(initExpr, lineNum);
            if (symbolRecord && symbolRecord->hasValue) {
                value = symbolRecord->constValue;
            } else {
                ErrorMessageWithNode(INIT_VALUE_ERROR_MSG, initExpr, lineNum);
            }
        } break;

        default: break;
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

/**
 * Generate Code for global variables that have initializers.
 * @param varDef
 */
void GC_Variable(const List *varDef) {
    char *varName = varDef->nodes[1].value.str;
    SymbolRecord *varSymRec = findSymbol(mainSymbolTable, varName);

    // does this variable definition have a list of initial values?
    int hasInitializer = (varDef->count >= 4) && (varDef->nodes[4].type == N_LIST);

    bool isNotAlias = (varSymRec->kind != SK_ALIAS);

    if (hasInitializer && isArray(varSymRec) && isNotAlias) {
        if (!isConst(varSymRec)) {
            ErrorMessageWithList("Non-const array cannot be initialized with data", varDef);
            return;
        }

        List *initList = varDef->nodes[4].value.list;
        if (initList->nodes[0].value.parseToken == PT_INIT) {
            ListNode valueNode = initList->nodes[1];
            if (valueNode.type == N_LIST) {
                List *initValueList = valueNode.value.list;

                if (initValueList->hasNestedList &&
                        isToken(initValueList->nodes[0], PT_LIST)) {
                    // need to loop over lists
                    for (int nestedCnt = 1; nestedCnt < initValueList->count; nestedCnt++) {
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

                OutputBlock *staticData = OB_AddData(varSymRec, varName, valueNode.value.list);

                // Mark where in memory the variable is located...
                //   TODO:  Doesn't seem this is the best place to do this if we want the blocks
                //            to be magically movable

                //printf("Size of %s is %d\n", varSymRec->name, staticData->blockSize);
                varSymRec->location = ICG_MarkStaticArrayData(staticData->blockSize);
                varSymRec->hasLocation = true;

                reverseData = false;
            }
        }
    }
}

void GC_StatementList(const List *code) {
    for (int stmtNum = 1; stmtNum < code->count; stmtNum++) {
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

/*** Preload any parameters into appropriate registers */
void GC_PreloadParams(SymbolTable *params) {
    SymbolRecord *curParam = params->firstSymbol;
    while (curParam != NULL) {
        if (curParam->hint != VH_NONE) {
            IL_Preload(curParam, curParam->hint);
        }
        curParam = curParam->next;
    }
}

void GC_ProcessFunction(char *funcName, List *code) {
    SymbolRecord *funcSym = findSymbol(mainSymbolTable, funcName);
    Label *funcLabel = newLabel(funcName, L_CODE);

    // start building function using provided label and current code address
    int funcAddr = ICG_StartOfFunction(funcLabel, funcSym);
    symbol_setAddr(funcSym, funcAddr);

    // load in local symbol table for function
    SymbolExt* funcExt = funcSym->funcExt;
    curFuncSymbolTable = funcExt->localSymbolSet;
    curFuncParamTable = funcExt->paramSymbolSet;

    setEvalLocalSymbolTable(curFuncSymbolTable);

    if (curFuncParamTable != NULL) {
        // need to let the code generator/instruction generator
        //  know about parameters that have been preloaded
        //   into registers
        GC_PreloadParams(curFuncParamTable);
    }

    GC_CodeBlock(code);
    funcExt->instrBlock = ICG_EndOfFunction(funcLabel);

    OB_AddCode(funcName, funcExt->instrBlock);
}

void GC_Function(const List *function, int codeNodeIndex) {
    char *funcName = function->nodes[1].value.str;

    if (function->count >= codeNodeIndex) {
        ListNode codeNode = function->nodes[codeNodeIndex];
        if (codeNode.type == N_LIST) {

            SymbolRecord *funcSym = findSymbol(mainSymbolTable, funcName);
            if (!isMainFunction(funcSym) && (funcSym->funcExt->cntUses == 0)) {
                printf("Skipping unused function %s\n", funcName);
            } else {
                printf("Processing function: %s\n", funcName);
                List *code = codeNode.value.list;
                GC_ProcessFunction(funcName, code);
            }
        } else {
            printf("Processing function definition: %s\n", funcName);
        }
    } else {
        ErrorMessage("Error processing function", funcName, function->lineNum);
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
    curFuncParamTable = NULL;
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

void GC_Program(List *program) {
    WalkProgram(program, &GC_ProcessProgramNode);
}

//-------------------------------------------------------------------
//===

static bool isInitialized = false;

void GC_ProcessProgram(ListNode node) {

    // make sure to reset error count
    GC_ErrorCount = 0;
    if (node.type == N_LIST) {
        List *program = node.value.list;
        if (program->nodes[0].value.parseToken == PT_PROGRAM) {
            // generate all the code for the program
            GC_Program(program);
        }
    }
}

void generate_code(char *name, ListNode node, SymbolTable *symbolTable) {
    printf("\nGenerating Code for %s\n", name);

    mainSymbolTable = symbolTable;
    curFuncSymbolTable = NULL;
    curFuncParamTable = NULL;
    reverseData = false;

    initEvaluator(mainSymbolTable);

    FE_initDebugger();

    // TODO: This code is still specific to Atari 2600... need to figure out a way to eliminate that constraint.
    // only need to initial instruction list and output block modules once
    if (!isInitialized) {
        IL_Init(0xF000);
        OB_Init();
        isInitialized = true;
    }

    // process module -> just needs to run code generator to build output blocks
    GC_ProcessProgram(node);

    FE_killDebugger();
}