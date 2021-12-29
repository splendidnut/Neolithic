//--------------------------------------------
//--------- GS: Generate Symbols -------------
//
// Generate symbol tables using Parse Tree
//
//  This module walks the AST early on to collect all the function and
//  other (vars/const/structs) declarations/definitions.
//
//  NOTE:  This module does not scan thru nested code blocks, so only
//         the top-level code blocks can have declarations in them.
//
// Created by pblackman on 3/27/2020.
//

#include <stdio.h>
#include <string.h>

#include "gen_symbols.h"
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

/**
 * Process Initializer portion of variable definition statement
 *
 * @param varDef
 * @param varSymRec
 */
void GS_processInitializer(const List *varDef, SymbolRecord *varSymRec) {
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
                setSymbolLocation(varSymRec, value, (value < 256) ? SS_ZEROPAGE : SS_ABSOLUTE);
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

/**
 * Process Variable definition statement
 *
 * @param varDef - syntax tree containing variable definition statement
 * @param symbolTable - symbol table for current scope
 * @param modFlags - modifier flags to apply for special use cases
 * @return symbolRecord for newly created variable symbol
 */
SymbolRecord *GS_Variable(List *varDef, SymbolTable *symbolTable, enum ModifierFlags modFlags) {
    char *varName = varDef->nodes[1].value.str;

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

        if ((userTypeSymbol != NULL) &&
                (isStruct(userTypeSymbol) || isUnion(userTypeSymbol) || isEnum(userTypeSymbol))) {
            symbolType = ST_STRUCT;
            childTable = GET_STRUCT_SYMBOL_TABLE(userTypeSymbol);
        } else {
            ErrorMessage("Unknown user-defined type", baseType, varDef->lineNum);
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

    List *modList = (varDef->nodes[3].type == N_LIST) ? varDef->nodes[3].value.list : NULL;
    bool isTypeModified = (modList != NULL) && (modList->count > 0);
    if (isTypeModified) {

        // loop thru Modifier List, and process the results, setting the appropriate flags for the symbol

        for (int modIndex = 0; modIndex < modList->count; modIndex++) {
            ListNode modNode = modList->nodes[modIndex];
            if (modNode.type == N_TOKEN) {
                switch (modNode.value.parseToken) {
                    case PT_ZEROPAGE: modFlags |= SS_ZEROPAGE; break;
                    case PT_ALIAS:    symbolKind = SK_ALIAS;   break;
                    case PT_CONST:    symbolKind = SK_CONST;   break;
                    case PT_UNSIGNED: modFlags |= ST_UNSIGNED; break;
                    case PT_SIGNED:   modFlags |= ST_SIGNED;   break;
                    case PT_REGISTER: modFlags |= SS_REGISTER; break;
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
    varSymRec->symbolTbl = childTable;

    // if there's a memory hint, use it
    if ((varDef->count > 5) && (varDef->nodes[5].type == N_INT)) {
        int addr = varDef->nodes[5].value.num;
        setSymbolLocation(varSymRec, addr, (addr < 256) ? SS_ZEROPAGE : SS_ABSOLUTE);
    }

    int hasInitializer = (varDef->count >= 4) && (varDef->nodes[4].type == N_LIST);
    if (hasInitializer && (symbolKind != SK_ALIAS)) {
        GS_processInitializer(varDef, varSymRec);
    }
    return varSymRec;
}


//----------------------------------------------------------------------------------------------
//  Function prototypes -- (since struct and union can be nested within themselves/each other)

void GS_Structure(List *structDef, SymbolTable *symbolTable);
int GS_ProcessUnionList(SymbolTable *symbolTable, const List *varList, int ofs);

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
            SymbolRecord *symRec = GS_Variable(varDef, symbolTable, 0);
            if (symRec != NULL) {
                printf("Adding union variable: %s @%d\n", symRec->name, ofs);
                int varSize = calcVarSize(symRec);
                setSymbolLocation(symRec, ofs, 0);
                if (varSize > maxElementSize) maxElementSize = varSize;
            }
        }
        index++;
    }
    return maxElementSize;
}

int GS_UnionWithName(List *varList, SymbolTable *symbolTable, char* unionName, int ofs) {
    SymbolRecord *unionSym = addSymbol(symbolTable, unionName, SK_UNION, ST_NONE, 0);

    SymbolTable *unionSymTbl = initSymbolTable(unionName, symbolTable);

    int unionSize = GS_ProcessUnionList(unionSymTbl, varList, ofs);
    setStructSize(unionSym, unionSize);
    unionSym->symbolTbl = unionSymTbl;
    return unionSize;
}


int GS_Union(List *unionDef, SymbolTable *symbolTable, int ofs) {
    if (unionDef->count < 3) return 0;

    // make sure we have a varList (otherwise it's an empty struct)
    List *varList = unionDef->nodes[2].value.list;
    if (varList->nodes[0].value.parseToken != PT_VARS) return 0;

    char *unionName = getDefName(unionDef);
    if (unionName != NULL) {
        return GS_UnionWithName(varList, symbolTable, unionName, ofs);
    } else {
        return GS_ProcessUnionList(symbolTable, varList, ofs);
    }
}

SymbolRecord * addSymbolForEnumType(SymbolTable *symbolTable, char *enumTypeName) {
    return addSymbol(symbolTable, enumTypeName, SK_ENUM, ST_CHAR, 0);
}

void addSymbolForEnumValue(SymbolTable *symbolTable, char *enumName, int enumValue) {

    // create the new variable for enumValue symbol as a 'const char'
    SymbolRecord *varSymRec = addSymbol(symbolTable, enumName, SK_CONST, ST_CHAR, MF_ENUM_VALUE);
    varSymRec->constValue = enumValue;
    varSymRec->hasValue = true;
}

void GS_Enumeration(List *enumDef, SymbolTable *symbolTable) {
    int enumCnt = enumDef->count;

    SymbolRecord *enumSymbol = NULL;
    SymbolTable *enumSymTbl = NULL;

    // Check to see if enumeration is named...
    //   IF so, then create symbol for enum + symbol table for the enum values.
    ListNode enumTypeNameNode = enumDef->nodes[1];
    if (enumTypeNameNode.type == N_STR) {
        enumSymbol = addSymbolForEnumType(symbolTable, enumTypeNameNode.value.str);
        enumSymTbl = initSymbolTable(enumSymbol->name, symbolTable);
        enumSymbol->symbolTbl = enumSymTbl;
    }

    // start index for enumeration list is determined by whether enumeration has a name or not
    int index = (enumSymbol ? 2 : 1);
    while (index < enumCnt) {
        List *enumNode = enumDef->nodes[index].value.list;
        char *enumName = enumNode->nodes[0].value.str;
        int enumValue = enumNode->nodes[1].value.num;

        addSymbolForEnumValue(symbolTable, enumName, enumValue);
        if (enumSymbol) {
            addSymbolForEnumValue(enumSymTbl, enumName, enumValue);
        }

        index++;
    }
}

//-----------------------------------------------
//  ['struct', name, ['varList', [ var1, var2 ] ] ]
void GS_Structure(List *structDef, SymbolTable *symbolTable) {
    char *structName;
    bool isAnonymousSt = false;

    if (structDef->nodes[1].type == N_STR) {
        structName = structDef->nodes[1].value.str;
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

        SymbolTable *structSymTbl = initSymbolTable(structName, symbolTable);

        for_range(index, 1, numVars) {
            List *varDef = varList->nodes[index].value.list;
            ListNode varDefType = varDef->nodes[0];

            if (isToken(varDefType, PT_UNION)) {
                int unionSize = GS_Union(varDef, structSymTbl, memOfs);
                memOfs += unionSize;
            } else {
                SymbolRecord *symRec = GS_Variable(varDef, structSymTbl, 0);
                if (symRec != NULL) {
                    setSymbolLocation(symRec, memOfs, 0);
                    memOfs += calcVarSize(symRec);
                }
            }
        }
        structSym->symbolTbl = structSymTbl;
    }

    setStructSize(structSym, memOfs);

    // if this was an anonymous structure, fold vars into parent structure
    if (isAnonymousSt) {

    }
}

//=====================================================================================
// --  Handle Function Parameter passing
//

void GS_FuncParamAlloc(const SymbolRecord *funcSym) {
    SymbolList *funcParamList = getParamSymbols(GET_LOCAL_SYMBOL_TABLE(funcSym));

    // first count stack params
    int numStackParams = 0;
    for_range (paramIdx, 0, funcParamList->count) {
        SymbolRecord *curParam = funcParamList->list[paramIdx];
        if (curParam->hint == VH_NONE) {
            numStackParams++;
        }
    }

    // NOTE: This is NEEDED for letting any function call know how much stack is used for params
    //         Mainly for cleanup purposes
    funcParamList->cntStackVars = numStackParams;

    // now assign stack locations
    int curStackPos = 0x100 + numStackParams + 2;   // +2 to skip return address bytes on stack

    for_range (paramIdx, 0, funcParamList->count) {
        SymbolRecord *curParam = funcParamList->list[paramIdx];
        if (curParam->hint == VH_NONE) {
            setSymbolLocation(curParam, curStackPos, SS_STACK);
            curStackPos--;
        }
    }
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
    SymbolTable *localVarTbl = NULL;
    char *funcName = funcDef->nodes[1].value.str;

    if (findSymbol(symbolTable, funcName)) {
        ErrorMessage("Duplicate function name", funcName, funcDef->lineNum);
        return;
    }
    funcSym = addSymbol(symbolTable, funcName, SK_FUNC, ST_NONE, MF_NONE);

    FM_addFunctionDef(funcSym);

    localVarTbl = initSymbolTable(funcName, symbolTable);

    //----------------------------------------------
    // generate param list symbol table

    ListNode funcParamNode = funcDef->nodes[4];
    int paramCnt = 0;
    bool hasParams = (funcParamNode.type == N_LIST && funcParamNode.value.list->hasNestedList);
    if (hasParams) {
        List *paramList = funcParamNode.value.list;
        paramCnt = paramList->count-1;

        if (paramCnt > 3) printf("\tToo many function parameters defined\n");

        // go thru parameter list and process entry as a variable
        for_range ( paramIndex, 1, paramList->count) {
            GS_Variable(paramList->nodes[paramIndex].value.list, localVarTbl, MF_PARAM | MF_LOCAL);
        }
    }

    //------------------------------------------------
    // Generate local symbol table if function has code block

    if (funcDef->count > 5) {
        ListNode funcCodeNode = funcDef->nodes[5];
        List *funcCode = funcCodeNode.value.list;

        // go thru function code block, find vars, and process them as local vars.
        for_range (funcStmtNum, 1, funcCode->count) {
            ListNode stmtNode = funcCode->nodes[funcStmtNum];
            if (stmtNode.type == N_LIST) {
                List *stmt = stmtNode.value.list;
                if (isToken(stmt->nodes[0], PT_DEFINE)) {
                    GS_Variable(stmt, localVarTbl, SS_ZEROPAGE | MF_LOCAL);
                }
            }
        }
    }

    //--- Remove table if there are no variables
    if (localVarTbl->firstSymbol == NULL) {
        killSymbolTable(localVarTbl);
        localVarTbl = NULL;
    }

    // ---------------------------
    //  Save all the hard work
    funcSym->symbolTbl = localVarTbl;

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
                        GS_Variable(statement, workingSymTbl, 0);
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
    initEvaluator(symbolTable);

    if (node.type == N_LIST) {
        List *program = node.value.list;
        if (isToken(program->nodes[0], PT_PROGRAM)) {
            GS_Program(program, symbolTable);
        }
    }
}