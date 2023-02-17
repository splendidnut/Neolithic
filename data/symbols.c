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

//---------------------------------------------------
//----------------- Symbol Table --------------------
//
//   Code to handle symbol tables
//
//   At this point, it's a bit overly complicated, partially due to the
//   hack job done when adding new features and some past attempts at
//   reorganizing the code.
//
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "symbols.h"
#include "data/instr_list.h"


//--------------------------------------------------------
//--- Symbol Kind code

const struct SymbolKindStruct {
    char *kindName;
    enum SymbolKind symbolKindEnum;
} SymbolKinds[] = {
        {"",       SK_NONE},    // used for labels
        {"var",    SK_VAR},
        {"alias",  SK_ALIAS},
        {"const",  SK_CONST},
        {"func",   SK_FUNC},
        {"struct", SK_STRUCT},
        {"union",  SK_UNION},
        {"enum",   SK_ENUM}
};

static const int NumSymbolKinds = sizeof(SymbolKinds) / sizeof(struct SymbolKindStruct);


char* getNameOfSymbolKind(enum SymbolKind symbolKind) {
    int i=0;
    do {
        if (SymbolKinds[i].symbolKindEnum == symbolKind)
            return SymbolKinds[i].kindName;
    } while (++i < NumSymbolKinds);
    return "";
}


//--------------------------------------------------------
//--- Symbol Type detection code


struct SymbolTypeList {
    char *name;
    enum SymbolType type;
    int flags;
} SymbolTypes[] = {
        {"char",    ST_CHAR,    ST_SIGNED},
        {"byte",    ST_CHAR,    ST_UNSIGNED},
        {"int",     ST_INT,     ST_SIGNED},
        {"word",    ST_INT,     ST_UNSIGNED},
        {"bool",    ST_BOOL,    0},
        {"boolean", ST_BOOL,    0},
};

const int NUM_DATA_TYPES = sizeof(SymbolTypes) / sizeof(struct SymbolTypeList);

enum SymbolType getSymbolType(const char *baseType) {
    enum SymbolType symbolType = ST_NONE;

    for (int index=0; index<NUM_DATA_TYPES; index++) {
        if (strncmp(baseType, SymbolTypes[index].name, 8) == 0)
            symbolType = SymbolTypes[index].type;
    }
    return symbolType;
}

//-----------------------------------------------------------------------
//   Internal function
//-----------------------------------------------------------------------

SymbolRecord * newSymbol(char *tname, enum SymbolKind kind, enum SymbolType type, unsigned int flags) {
    SymbolRecord *newSymbol = (SymbolRecord *)allocMem(sizeof(SymbolRecord));
    memset(newSymbol, 0, sizeof(SymbolRecord));

    newSymbol->name = tname;
    newSymbol->kind = kind;
    newSymbol->flags = flags | type;
    newSymbol->location = -1;           // indicate that location has not been set
    newSymbol->constEvalNotes = "";

    return newSymbol;
}



/*-----------------------------------------------*/
/*    Public functions                           */
/*-----------------------------------------------*/

SymbolRecord * addConst(SymbolTable *symbolTable, char *name, int value, enum SymbolType type, enum ModifierFlags flags) {
    SymbolRecord *varSymRec;
    varSymRec = addSymbol(symbolTable, name, SK_CONST, type, flags);
    varSymRec->constValue = value;
    varSymRec->hasValue = true;
    return varSymRec;
}

SymbolTable *initSymbolTable(char *name, SymbolTable *parentTable) {
    SymbolTable *newTable = allocMem(sizeof(SymbolTable));
    newTable->name = name;
    newTable->parentTable = parentTable;
    newTable->firstSymbol = NULL;
    newTable->lastSymbol = NULL;

    if (parentTable == NULL) {
        // add built-in symbols if this is the global table
        addConst(newTable, "false", 0, ST_BOOL, 0);
        addConst(newTable, "true",  1, ST_BOOL, 0);
    }
    return newTable;
}


SymbolRecord * addSymbol(SymbolTable *symbolTable, char *name,
        enum SymbolKind kind, enum SymbolType type, unsigned int flags) {

    /* first try to find the symbol */
    SymbolRecord *symbol = findSymbol(symbolTable, name);

    /* only create symbol if it doesn't already exist */
    if (symbol == NULL) {
        symbol = newSymbol(name, kind, type, flags);
        if (symbolTable->firstSymbol != NULL) {
            // add to end of symbol table
            symbolTable->lastSymbol->next = symbol;
            symbolTable->lastSymbol = symbol;
        } else {
            /* first symbol marks both the beginning and end of symbol table */
            symbolTable->firstSymbol = symbol;
            symbolTable->lastSymbol = symbol;
        }
    } else {
        /* override existing symbol */
        printf("WARNING:  duplicate symbol: %s\n", name);
    }
    return symbol;
}

void setSymbolArraySize(SymbolRecord *symbol, int arraySize) {
    symbol->numElements = arraySize;
}

void setSymbolLocation(SymbolRecord *symbolRecord, int location, enum ModifierFlags storageType) {
    if (symbolRecord == NULL) return;

    symbolRecord->location = location;
    symbolRecord->flags = (symbolRecord->flags & (~SS_STORAGE_MASK)) | storageType;
}


void setStructSize(SymbolRecord *symbol, int unionSize) {
    symbol->numElements = unionSize;
}

unsigned char getStructVarSize(const SymbolRecord *symbol) {
    return calcVarSize(symbol->userTypeDef);
}

void markFunctionUsed(SymbolRecord *funcSymbol) {
    if (isFunction(funcSymbol)) {
        funcSymbol->cntUses++;
    }
}

/**
 * Calculate the total size of the provided variable (memory space taken in bytes)
 */
int calcVarSize(const SymbolRecord *varSymRec) {// calculate allocation size
    int varSize = 1;

    bool isPtr = (varSymRec->flags & MF_POINTER);
    bool isUserDefined = (varSymRec->userTypeDef != NULL);
    bool isArray = (varSymRec->flags & MF_ARRAY);
    bool isStruct = (varSymRec->kind == SK_STRUCT);

    // if user defined type (non-pointer var), look up size of type
    if (isUserDefined && !isPtr) {
        varSize = calcVarSize(varSymRec->userTypeDef);
    }

    if (isArray || isStruct) varSize = varSize * varSymRec->numElements;

    if ((varSymRec->flags & ST_MASK) == ST_INT || isPtr) varSize = varSize * 2;
    return varSize;
}

int getBaseVarSize(const SymbolRecord *varSymRec) {
    if (varSymRec->userTypeDef != NULL) return getStructVarSize(varSymRec);
    bool isPointer = (varSymRec->flags & MF_POINTER);
    bool isInt = ((varSymRec->flags & ST_MASK) == ST_INT);
    return (isPointer || isInt) ? 2 : 1;
}

int getCodeSize(const SymbolRecord *funcSymRec) {
    bool isFunc = isFunction(funcSymRec);
    bool isCodeUsed = IS_FUNC_USED(funcSymRec);
    return (isFunc && isCodeUsed && (funcSymRec->instrBlock != NULL)) ? funcSymRec->instrBlock->codeSize : 0;
}

int calcCodeSize(const SymbolRecord *funcSymRec) {
    bool isFunc = isFunction(funcSymRec);
    return (isFunc && (funcSymRec->instrBlock != NULL)) ? funcSymRec->instrBlock->codeSize : 0;
}

SymbolRecord * findSymbol(SymbolTable *symbolTable, const char* name) {
    if (name[0] == '\0') return NULL;
    SymbolRecord *curSymbol = symbolTable->firstSymbol;
    while (curSymbol != NULL) {
        char *curName = curSymbol->name;
        if (curName && (strncmp(curSymbol->name, name, SYMBOL_NAME_LIMIT) == 0))
            return curSymbol;
        curSymbol = curSymbol->next;
    }
    return curSymbol;
}

char getDestRegFromHint(enum VarHint hint) {
    char destReg;
    switch (hint) {
        case VH_A_REG: destReg = 'A'; break;
        case VH_X_REG: destReg = 'X'; break;
        case VH_Y_REG: destReg = 'Y'; break;
        default:
            destReg = 'S';
    }
    return destReg;
}

/**
 * Find all parameter-type variables from the provided symbol table
 *
 * @param symTblWithParams - symbol table containing the parameters
 * @return SymbolList of all the variables which are function parameters
 */
SymbolList *getParamSymbols(SymbolTable *symTblWithParams) {
    if (symTblWithParams == NULL) return NULL;

    SymbolList *paramList = malloc(sizeof(SymbolList));
    paramList->count = 0;
    paramList->cntStackVars = 0;

    SymbolRecord *firstSymbol = symTblWithParams->firstSymbol;
    while (firstSymbol != NULL) {
        if (IS_PARAM_VAR(firstSymbol)) {
            paramList->list[paramList->count] = firstSymbol;
            paramList->count++;

            if (IS_STACK_VAR(firstSymbol)) paramList->cntStackVars++;
        }
        firstSymbol = firstSymbol->next;
    }
    return paramList;
}

SymbolRecord *lookupProperty(SymbolRecord *structSymbol, char *propName) {
    if (structSymbol->userTypeDef != NULL) {
        return findSymbol(getStructSymbolSet(structSymbol), propName);
    } else {
        return findSymbol(structSymbol->symbolTbl, propName);
    }
}


/**
 * isSimpleConst - is this a constant that has a singular value?
 * @param symbol
 * @return
 */
bool isSimpleConst(SymbolRecord *symbol) {
    unsigned int flags = symbol->flags;
    return (symbol->kind == SK_CONST) && !(flags & (MF_ARRAY | ST_STRUCT));
}

bool isArrayConst(SymbolRecord *symbol) {
    return isConst(symbol) && isArray(symbol);
}

//----------------------------------------------------------------------
//--- Checks for Symbol Kind

bool isConst(const SymbolRecord *symbol) {
    return (symbol->kind == SK_CONST);
}

bool isVariable(const SymbolRecord *symbol) {
    return (symbol->kind == SK_VAR);
}

bool isFunction(const SymbolRecord *symbol) {
    return (symbol->kind == SK_FUNC);
}

bool isStruct(const SymbolRecord *symbol) {
    return (symbol->kind == SK_STRUCT);
}

bool isUnion(const SymbolRecord *symbol) {
    return (symbol->kind == SK_UNION);
}

bool isEnum(const SymbolRecord *symbol) {
    return (symbol->kind == SK_ENUM);
}

//--- Checks for Symbol flags

bool isPointer(const SymbolRecord *symbol) {
    return (symbol->flags & MF_POINTER) != 0;
}

bool isArray(const SymbolRecord *symbol) {
    return (symbol->flags & MF_ARRAY) != 0;
}

//---- other checks

bool isMainFunction(const SymbolRecord *symbol) {
    return (strcmp(symbol->name, compilerOptions.entryPointFuncName)==0);
}

bool isSystemFunction(const SymbolRecord *symbol) {
    return (strcmp(symbol->name, "irq")==0)
        || (strcmp(symbol->name, "nmi")==0);
}

enum SymbolType getType(const SymbolRecord *symbol) {
    return (symbol->flags & ST_MASK);
}

void killSymbolTable(SymbolTable *symbolTable) {
    symbolTable->firstSymbol = NULL;
    symbolTable->lastSymbol = NULL;
    free(symbolTable);
}



//------------------------------------------------------------------------------------------
//---- Symbol Table printing/output


void printSymbol(FILE *outputFile, const SymbolRecord *curSymbol, int indentLevel) {
    int indentSize = indentLevel * 2;

    // indent labels if indentLevel > 0
    char symName[48];       // name buffer only used for print statement...
    if (indentLevel > 0) {
        for (int i=0; i<indentLevel*2; i++) symName[i] = ' ';
    }
    strcpy(symName + indentSize, curSymbol->name);

    char value[6] = "";
    if (curSymbol->hasValue) {
        sprintf(value, "%4x", curSymbol->constValue);
    }

    char location[6] = "";
    if (HAS_SYMBOL_LOCATION(curSymbol)) {
        sprintf(location, "%04x", curSymbol->location);
    }

    char userTypeName[32] = "";
    if (curSymbol->userTypeDef != NULL) {
        sprintf(userTypeName, "%s", curSymbol->userTypeDef->name);
    }

    // print information
    bool isFunc = isFunction(curSymbol);
    int baseSize = getBaseVarSize(curSymbol);
    int size = isFunc ? getCodeSize(curSymbol) : calcVarSize(curSymbol);
    fprintf(outputFile,
            " %-32s  %5s  %6s  %04x  %5s  %02x  %02x  %04x  %6s  %20s\n",
            symName,
            location,
            getNameOfSymbolKind(curSymbol->kind),
            curSymbol->flags,
            ((curSymbol->flags & MF_POINTER) != 0) ? "true" : "false",
            baseSize,
            curSymbol->numElements,
            size,
            value,
            userTypeName);

}

void printSubSymbolSet(FILE *outputFile, SymbolRecord *curSymbol, int indentLevel);

void printSubSymbolTable(FILE *outputFile, const SymbolTable *structSymTbl, int indentLevel) {
    if (structSymTbl != NULL) {
        SymbolRecord *structSymbol = structSymTbl->firstSymbol;
        while (structSymbol) {
            printSymbol(outputFile, structSymbol, indentLevel);
            printSubSymbolSet(outputFile, structSymbol, indentLevel);
            structSymbol = structSymbol->next;
        }
    }
}


void printSubSymbolSet(FILE *outputFile, SymbolRecord *curSymbol, int indentLevel) {
    if ((curSymbol->kind == SK_STRUCT) ||
        (curSymbol->kind == SK_UNION)) {
        SymbolTable *structSymTbl = GET_STRUCT_SYMBOL_TABLE(curSymbol);
        printSubSymbolTable(outputFile, structSymTbl, indentLevel + 1);
    } else if (isFunction(curSymbol)) {
        SymbolTable *localSymTbl = GET_LOCAL_SYMBOL_TABLE(curSymbol);
        if (localSymTbl != NULL) {
            fprintf(outputFile, "  Locals:\n");
            printSubSymbolTable(outputFile, localSymTbl, indentLevel + 2);
        }
    }
}

void printSymbolHeader(FILE *outputFile) {
    fprintf(outputFile, "    Symbol Name                    Loc    Kind  Flags  Pntr   BS  #El  Size  Value\n");
    fprintf(outputFile, "-----------------------------------------------------------------------------------\n");
}

void printSingleSymbol(FILE *outputFile, const SymbolRecord *curSymbol) {
    printSymbol(outputFile, curSymbol, 0);
}

void showSymbolTable(FILE *outputFile, SymbolTable *symbolTable) {
    SymbolRecord *curSymbol;

    fprintf(outputFile, "Symbol Table: \n");

    curSymbol = symbolTable->firstSymbol;
    if (!(curSymbol)) {
        fprintf(outputFile, "  (none)  \n");
    } else {
        printSymbolHeader(outputFile);
        do {
            printSymbol(outputFile, curSymbol, 0);
            printSubSymbolSet(outputFile, curSymbol, 0);
            curSymbol = curSymbol->next;
        } while (curSymbol);
    }
    fprintf(outputFile, "\n");
}

bool isStructDefined(const SymbolRecord *structSymbol) {
    return (structSymbol && GET_STRUCT_SYMBOL_TABLE(structSymbol));
}

/*SymbolTable *getStructSymbolSet(const SymbolRecord *structSymbol) {
    if (structSymbol->userTypeDef != NULL) {
        return GET_STRUCT_SYMBOL_TABLE(structSymbol->userTypeDef);
    } else {
        return NULL;
    }
}*/

// NOTE: This is a variable name getter designed specifically to handle DASM style local labels.
// TODO: Figure out a better way to handle this.
const char *getVarName(const SymbolRecord *varSym) {
    if (IS_LOCAL(varSym)) {
        char *varName = allocMem(strlen(varSym->name)+2);
        strcpy(varName, ".");
        return strcat(varName, varSym->name);
    } else {
        return varSym->name;
    }
}