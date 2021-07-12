//--------------------------------------------
//--------- GS: Generate Symbols -------------
//
// Generate symbol tables using Parse Tree
//
// Created by pblackman on 3/27/2020.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data/symbols.h"
#include "data/syntax_tree.h"
#include "machine/mem.h"

#include "codegen/gen_common.h"
#include "codegen/eval_expr.h"
#include "data/func_map.h"


/**
 * Get definition name from a definition list
 * @param def
 * @return NULL if no definition name
 */
char* getDefName(List *def) {
    return (((def->count > 1) && (def->nodes[1].type == N_STR))
            ? def->nodes[1].value.str
            : NULL);
}

//-----------------------------------------------------
//  Function prototypes

void GS_Structure(List *structDef, SymbolTable *symbolTable);
int GS_Union(List *unionDef, SymbolTable *symbolTable, int ofs);
void GS_Enumeration(List *enumDef, SymbolTable *symbolTable);

//-----------------------------------------------------

SymbolRecord * GS_Variable(List *varDef, SymbolTable *symbolTable) {
    unsigned int modFlags = 0;
    char *varName = strdup(varDef->nodes[1].value.str);

#ifdef DEBUG_GEN_SYMBOLS
    printf("Variable: %s  ", varName);
#endif

    // figure out basic type/kind information
    List *typeList = varDef->nodes[2].value.list;
    char *baseType = typeList->nodes[0].value.str;
    enum SymbolKind symbolKind = SK_VAR;
    enum SymbolType symbolType = getSymbolType(baseType);

#ifdef DEBUG_GEN_SYMBOLS
    printf("  type: %s  ", baseType);
#endif

    // check for user defined type and process it
    SymbolTable *childTable = NULL;
    SymbolRecord *userTypeSymbol = NULL;
    if (symbolType == ST_NONE) {

        // look up user defined type
        userTypeSymbol = findSymbol(symbolTable, baseType);
        if (userTypeSymbol == NULL && symbolTable->parentTable != NULL) {
            userTypeSymbol = findSymbol(symbolTable->parentTable, baseType);
        }

        if (userTypeSymbol != NULL) {
            if (isStruct(userTypeSymbol)) {
                symbolType = ST_STRUCT;
                childTable = userTypeSymbol->funcExt->paramSymbolSet;
            } else if (isUnion(userTypeSymbol)) {
                symbolType = ST_STRUCT;
                childTable = userTypeSymbol->funcExt->paramSymbolSet;
            }
        } else {
            printf("Unknown symbol type: '%s'\n", baseType);
            showSymbolTable(stdout, symbolTable);
        }
    }

    //--------------------------------------------------
    // deal with pointer/array indicators as modifiers
    int arraySize = 1;

    char hint = 0;
    bool isTypeExt = (typeList->count > 1);
    if (isTypeExt) {
        ListNode node = typeList->nodes[1];
        bool isPointer = isToken(node, PT_PTR);
        if (isPointer) {
            modFlags |= MF_POINTER;
            if (typeList->count > 2) node = typeList->nodes[2];
        }

        bool isArray = isToken(node, PT_ARRAY) || (node.type == N_INT) || (node.type == N_STR);
        if (isArray) {
            modFlags |= MF_ARRAY;
            if (node.type == N_INT) {
                // need to save array size in symbol table
                arraySize = node.value.num;
            } else if (node.type == N_STR) {
                SymbolRecord *arraySizeConst = findSymbol(symbolTable, node.value.str);
                if (arraySizeConst && isSimpleConst(arraySizeConst)) {
                    arraySize = arraySizeConst->constValue;
                } else {
                    printf("Unknown array size: %s\n", node.value.str);
                }
            }
            if (typeList->count > 3) node = typeList->nodes[3];
        }

        bool hasHint = (node.type == N_LIST && node.value.list->nodes[0].value.parseToken == PT_HINT);
        if (hasHint) {
            hint = node.value.list->nodes[1].value.str[0];
        }
    }

    List *modList = varDef->nodes[3].value.list;
    bool isTypeModified = (modList != NULL) && (modList->count > 0);
    if (isTypeModified) {

        // loop thru Modifier List, and process the results, setting the appropriate flags for the symbol

        for (int modIndex = 0; modIndex < modList->count; modIndex++) {
            ListNode modNode = modList->nodes[modIndex];
            if (modNode.type == N_TOKEN) {
                switch (modNode.value.parseToken) {
                    case PT_ZEROPAGE: modFlags |= MF_ZEROPAGE; break;
                    case PT_CONST:    symbolKind = SK_CONST;   break;
                    case PT_UNSIGNED: modFlags |= ST_UNSIGNED; break;
                    case PT_SIGNED:   modFlags |= ST_SIGNED;   break;
                    case PT_REGISTER: modFlags |= MF_REGISTER; break;
                    case PT_INLINE:   modFlags |= MF_INLINE;   break;
                }
            } else {
                printf("Unknown modifier: %s\n", modNode.value.str);
            }
        }
    }

#ifdef DEBUG_GEN_SYMBOLS
    showList(varDef->nodes[3].value.list, 1);
    printf("\n");
#endif

    // create the new variable
    SymbolRecord *varSymRec = addSymbol(symbolTable, varName, symbolKind, symbolType, modFlags);
    varSymRec->isLocal = false;
    varSymRec->userTypeDef = userTypeSymbol;
    setSymbolArraySize(varSymRec, arraySize);

    // process hint if available
    if (hint > 0) {
        switch (hint) {
            case 'A': varSymRec->hint = VH_A_REG; break;
            case 'X': varSymRec->hint = VH_X_REG; break;
            case 'Y': varSymRec->hint = VH_Y_REG; break;
        }
    }

    // if variable uses a struct/union, add child table
    if (childTable != NULL) {
        addSymbolExt(varSymRec, childTable, NULL);
    }

    // if there's a memory hint, use it
    if ((varDef->count > 5) && (varDef->nodes[5].type == N_INT)) {
        addSymbolLocation(varSymRec, varDef->nodes[5].value.num);
    }

    int hasInitializer = (varDef->count >= 4) && (varDef->nodes[4].type == N_LIST);
    if (hasInitializer) {
        List* initList = varDef->nodes[4].value.list;
        if (initList->nodes[0].value.parseToken == PT_INIT) {
            ListNode valueNode = initList->nodes[1];

            if (valueNode.type == N_INT) {
                // simply set value
                varSymRec->constValue = valueNode.value.num;
                varSymRec->hasValue = true;
            }

            else if (valueNode.type == N_LIST) {
                List *initExpr = valueNode.value.list;

                if (initExpr->hasNestedList && isToken(initExpr->nodes[0], PT_LIST)) {
                    varSymRec->numElements = initExpr->count - 1;
                } else {
                    varSymRec->numElements = initExpr->count;
                }

                /**** SPECIAL CASE: handle case of explicit address set  ****/
                if (isToken(initExpr->nodes[0], PT_ADDR_OF)
                    && (initExpr->nodes[1].type == N_INT)) {

                    int value = initExpr->nodes[1].value.num;
                    addSymbolLocation(varSymRec, value);
                } else {
                    EvalResult result = evaluate_expression(initExpr);
                    if (result.hasResult) {
                        varSymRec->constValue = result.value;
                        varSymRec->hasValue = true;
                        varSymRec->constEvalNotes = get_expression(initExpr);
                    } else if(isSimpleConst(varSymRec)) {
                        varSymRec->constValue = 0;
                        varSymRec->constEvalNotes = "(unable to resolve)";

                        ErrorMessageWithList("Unable to resolve", initExpr);
                    }
                }
            }
        }
    }
    return varSymRec;
}

//-----------------------------------------------------
// ['union', EMPTY, ['varList', [ var1, var2 ] ] ]
// ['union', name, ['varList', [ var1, var2 ] ] ]

int GS_ProcessUnionList(SymbolTable *symbolTable, const List *varList, int ofs) {
    int numVars = varList->count; // vars in structure/union
    int maxElementSize = 0;
    int index = 1;
    while (index < numVars) {
        List *varDef = varList->nodes[index].value.list;
        ListNode varDefType = varDef->nodes[0];

        if (isToken(varDefType, PT_UNION)) {
            GS_ProcessUnionList(symbolTable, varDef, ofs);
        } else if (isToken(varDefType, PT_STRUCT)) {
            GS_Structure(varDef, symbolTable);
        } else {
            SymbolRecord *symRec = GS_Variable(varDef, symbolTable);
            printf("Adding union variable: %s @%d\n", symRec->name, ofs);
            if (symRec != NULL) {
                int varSize = calcVarSize(symRec);
                addSymbolLocation(symRec, ofs);
                if (varSize > maxElementSize) maxElementSize = varSize;
            }
        }
        index++;
    }
    return maxElementSize;
}

int GS_UnionWithName(List *varList, SymbolTable *symbolTable, char* unionName, int ofs) {
    SymbolRecord *unionSym = addSymbol(symbolTable, unionName, SK_UNION, ST_NONE, 0);

    SymbolTable *unionSymTbl = initSymbolTable(unionName, false);
    unionSymTbl->parentTable = symbolTable;

    int unionSize = GS_ProcessUnionList(unionSymTbl, varList, ofs);
    setStructSize(unionSym, unionSize);
    addSymbolExt(unionSym, unionSymTbl, NULL);
    return unionSize;
}


int GS_Union(List *unionDef, SymbolTable *symbolTable, int ofs) {
    if (unionDef->count < 3) return 0;

    // make sure we have a varList (otherwise it's an empty struct)
    List *varList = unionDef->nodes[2].value.list;
    if (varList->nodes[0].value.parseToken != PT_VARS) return 0;

    char *unionName = getDefName(unionDef);
    if (unionName != NULL) {
        return GS_UnionWithName(varList, symbolTable, strdup(unionName), ofs);
    } else {
        return GS_ProcessUnionList(symbolTable, varList, ofs);
    }
}

void addSymbolForEnumType(SymbolTable *symbolTable, char *enumTypeName) {

    // create the new variable for enumType symbol
    SymbolRecord *varSymRec = addSymbol(symbolTable, enumTypeName, SK_ENUM, ST_CHAR, 0);
    varSymRec->isLocal = false;
}

void addSymbolForEnumValue(SymbolTable *symbolTable, char *enumName, int enumValue) {

    // create the new variable for enumValue symbol as a 'const char'
    SymbolRecord *varSymRec = addSymbol(symbolTable, enumName, SK_CONST, ST_CHAR, MF_ENUM_VALUE);
    varSymRec->isLocal = false;
    varSymRec->constValue = enumValue;
    varSymRec->hasValue = true;
}

void GS_Enumeration(List *enumDef, SymbolTable *symbolTable) {
    int enumCnt = enumDef->count;

    ListNode enumTypeNameNode = enumDef->nodes[1];
    if (enumTypeNameNode.type == N_STR) {
        addSymbolForEnumType(symbolTable, enumTypeNameNode.value.str);
    }

    int index = 2;
    while (index < enumCnt) {
        List *enumNode = enumDef->nodes[index].value.list;
        char *enumName = strdup(enumNode->nodes[0].value.str);
        int enumValue = enumNode->nodes[1].value.num;

        addSymbolForEnumValue(symbolTable, enumName, enumValue);

        index++;
    }
}

//-----------------------------------------------
//  ['struct', name, ['varList', [ var1, var2 ] ] ]
void GS_Structure(List *structDef, SymbolTable *symbolTable) {
    char *structName;
    bool isAnonymousSt = false;

    if (structDef->nodes[1].type == N_STR) {
        structName = strdup(structDef->nodes[1].value.str);
    } else {
        structName = "__temp";
        isAnonymousSt = true;
    }

    int memOfs = 0;

    SymbolRecord *structSym = addSymbol(symbolTable, structName, SK_STRUCT, ST_NONE, 0);

    if (structDef->count >= 3) {
        List *varList = structDef->nodes[2].value.list;

        // make sure we have a varList (otherwise it's an empty struct)
        if (varList->nodes[0].value.parseToken != PT_VARS) return;

        int numVars = varList->count; // vars in struct...

        SymbolTable *structSymTbl = initSymbolTable(structName, false);
        structSymTbl->parentTable = symbolTable;

        for_range(index, 1, numVars) {
            List *varDef = varList->nodes[index].value.list;
            ListNode varDefType = varDef->nodes[0];

            if (isToken(varDefType, PT_UNION)) {
                int unionSize = GS_Union(varDef, structSymTbl, memOfs);
                memOfs += unionSize;
            } else {
                SymbolRecord *symRec = GS_Variable(varDef, structSymTbl);
                if (symRec != NULL) {
                    addSymbolLocation(symRec, memOfs);
                    memOfs += calcVarSize(symRec);
                }
            }
        }
        addSymbolExt(structSym, structSymTbl, NULL);
    }

    setStructSize(structSym, memOfs);

    // if this was an anonymous structure, fold vars into parent structure
    if (isAnonymousSt) {

    }
}

//=====================================================================================
// --  Handle Function Parameter passing
//
//-- TODO:  So now, with the hint system, how do we solve the parameter passing debacle?
//
//  Usual parameters: P1 -> A,  P2 -> X,  P3 -> Y,  P4+ -> Push on Stack
//

enum RegParams { PARAM_A = 0x1, PARAM_X = 0x2, PARAM_Y = 0x4 };     // All available registers.

enum RegParams getAvailableParams(const SymbolTable *funcParams) {
    enum RegParams availParams = PARAM_A | PARAM_X | PARAM_Y;           // start off with all available

    // first collect and allocate based on current hints
    SymbolRecord *curParam = funcParams->firstSymbol;
    while (curParam != NULL) {
        switch (curParam->hint) {
            case VH_A_REG: availParams &= ~PARAM_A; break;
            case VH_X_REG: availParams &= ~PARAM_X; break;
            case VH_Y_REG: availParams &= ~PARAM_Y; break;
            default: break;
        }
        curParam = curParam->next;
    }
    return availParams;
}

void assignRemainingHints(const SymbolTable *funcParams, enum RegParams *availParams) {
    SymbolRecord *curParam = funcParams->firstSymbol;
    while (curParam != NULL) {
        if (curParam->hint == VH_NONE) {
            if ((*availParams) & PARAM_A) {
                curParam->hint = VH_A_REG;
                (*availParams) &= PARAM_A ^ 0xFF;
            } else if ((*availParams) & PARAM_X) {
                curParam->hint = VH_X_REG;
                (*availParams) &= PARAM_X  ^ 0xFF;
            } else if ((*availParams) & PARAM_Y) {
                curParam->hint = VH_Y_REG;
                (*availParams) &= PARAM_Y  ^ 0xFF;
            } else {
                printf("ERROR: Out of hints");
            }
        }
        curParam = curParam->next;
    }
}

void assignParamStackPos(const SymbolTable *funcParams) {
    SymbolRecord *curParam = funcParams->firstSymbol;

    // first count stack params
    int numStackParams = 0;
    while (curParam != NULL) {
        if (curParam-> hint == VH_NONE) numStackParams++;
        curParam = curParam->next;
    }

    // now assign stack locations
    int curStackPos = 2 + numStackParams;   // +2 to skip return address bytes on stack
    curParam = funcParams->firstSymbol;
    while (curParam != NULL) {
        if (curParam-> hint == VH_NONE) {
            curParam->isStack = true;
            curParam->hasLocation = true;
            curParam->location = curStackPos--;
        } else {
            curParam->isStack = false;
        }
        curParam = curParam->next;
    }
}

void GS_FuncParamAlloc(const SymbolRecord *funcSym) {
    SymbolTable *funcParams = funcSym->funcExt->paramSymbolSet;

    //  If there are hints, use them first...
    enum RegParams availParams = getAvailableParams(funcParams);

    // next assign any missing hints (HACK)
    //assignRemainingHints(funcParams, &availParams);

    // next assign stack spots
    assignParamStackPos(funcParams);
}

/**
 * Process both function definitions and implementations
 *
 * @param funcDef - root parse tree node of function
 * @param symbolTable - symbol table to stick definition in
 * @param isDefinition - is this just a definition?  (false = full function implementation)
 */
void GS_Function(List *funcDef, SymbolTable *symbolTable) {
    SymbolRecord *funcSym;
    SymbolTable *paramListTbl = NULL;
    SymbolTable *localVarTbl = NULL;
    char *funcName = strdup(funcDef->nodes[1].value.str);

    funcSym = addSymbol(symbolTable, funcName, SK_FUNC, ST_NONE, MF_NONE);

    //----------------------------------------------
    // generate param list symbol table

    ListNode funcParamNode = funcDef->nodes[4];
    int paramCnt = 0;
    bool hasParams = (funcParamNode.type == N_LIST && funcParamNode.value.list->hasNestedList);
    if (hasParams) {
        List *paramList = funcParamNode.value.list;
        paramCnt = paramList->count-1;

        if (paramCnt > 3) printf("\tToo many function parameters defined\n");

        paramListTbl = initSymbolTable(funcName, false);
        paramListTbl->parentTable = symbolTable;

        // go thru parameter list and process entry as a variable
        for_range ( paramIndex, 1, paramList->count) {
            SymbolRecord *paramVar = GS_Variable(paramList->nodes[paramIndex].value.list, paramListTbl);
            paramVar->flags |= MF_PARAM;
        }
    }

    //------------------------------------------------
    // Generate local symbol table if function has code block

    if (funcDef->count > 5) {
        ListNode funcCodeNode = funcDef->nodes[5];
        List *funcCode = funcCodeNode.value.list;

        localVarTbl = initSymbolTable(funcName, false);
        localVarTbl->parentTable = symbolTable;

        // go thru function code block, find vars, and process them as local vars.
        for_range (funcStmtNum, 1, funcCode->count) {
            ListNode stmtNode = funcCode->nodes[funcStmtNum];
            if (stmtNode.type == N_LIST) {
                List *stmt = stmtNode.value.list;
                if (isToken(stmt->nodes[0], PT_DEFINE)) {
                    SymbolRecord *newVar = GS_Variable(stmt, localVarTbl);
                    if (newVar != NULL) {
                        newVar->isLocal = true;
                        newVar->flags |= MF_ZEROPAGE;   // func local vars always in Zeropage
                    }
                }
            }
        }

        if (localVarTbl->firstSymbol == NULL) {
            killSymbolTable(localVarTbl);
            localVarTbl = NULL;
        }
    }

    // ---------------------------
    //  Save all the hard work
    addSymbolExt(funcSym, paramListTbl, localVarTbl);
    funcSym->funcExt->cntParams = paramCnt;

    // If we have parameters, make sure to do the allocations
    if (paramCnt > 0) GS_FuncParamAlloc(funcSym);
}


void GS_Program(List *list, SymbolTable *workingSymTbl) {
    for_range (stmtNum, 1, list->count) {
        ListNode stmt = list->nodes[stmtNum];
        if (stmt.type == N_LIST) {
            List *statement = stmt.value.list;
            if (statement->nodes[0].type == N_TOKEN) {
                enum ParseToken token = statement->nodes[0].value.parseToken;
                switch (token) {
                    case PT_DEFINE:
                        GS_Variable(statement, workingSymTbl);
                        break;
                    case PT_DEFUN:
                    case PT_FUNCTION:
                        GS_Function(statement, workingSymTbl);
                        break;
                    case PT_STRUCT:
                        GS_Structure(statement, workingSymTbl);
                        break;
                    case PT_UNION:
                        GS_Union(statement, workingSymTbl, 0);
                        break;
                    case PT_ENUM:
                        GS_Enumeration(statement, workingSymTbl);
                        break;
                }
            }
        }
    }
}

//--------------------------------------------------------------------------
// Walk thru the program node provided to add all symbols to the symbol table

void generate_symbols(ListNode node, SymbolTable *symbolTable) {
    printf("Finding all symbols\n");

    initEvaluator(symbolTable);

    if (node.type == N_LIST) {
        List *program = node.value.list;
        if (isToken(program->nodes[0], PT_PROGRAM)) {
            GS_Program(program, symbolTable);
        }
    }
}