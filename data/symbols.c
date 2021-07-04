//---------------------------------------------------
//----------------- Symbol Table --------------------
//
//   Code to handle symbol tables
//
//   At this point, it's a bit overly complicated, partially due to my attempt
//   to keep this somewhat 16-bit-ish for old DOS compilers.  The other reason
//   is due the hack job done when adding new features and some past attempts
//   at reorganizing the code.
//
//   TODO:
//      Cleanup!  Need to potentially rework/separate the Symbol flags into
//      there separate functions.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "symbols.h"
#include "data/instr_list.h"


const struct SymbolKindStruct {
    char *kindName;
    enum SymbolKind symbolKindEnum;
} SymbolKinds[] = {
        {"",       SK_NONE},    // used for labels
        {"var",    SK_VAR},
        {"const",  SK_CONST},
        {"func",   SK_FUNC},
        {"struct", SK_STRUCT},
        {"union",  SK_UNION},
        {"enum",   SK_ENUM}
};

static const int NumSymbolKinds = sizeof(SymbolKinds) / sizeof(struct SymbolKindStruct);

char *getSymbolKindName(enum SymbolKind symbolKind) {
    int i=0;
    while (i < 7) {
        if (SymbolKinds[i].symbolKindEnum == symbolKind) {
            return SymbolKinds[i].kindName;
        }
        i++;
    }
    return "";
}

/*-------------------------------------*/
/*   Internal functions declarations   */

static SymbolRecord * newSymbol(char *tname, enum SymbolKind kind, enum SymbolType type, unsigned int flags);
static void killSymbol(SymbolRecord *symbol);
static char* lookupSymbolKind(enum SymbolKind symbolKind);



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



/*-----------------------------------------------*/
/*    Public functions                           */
/*-----------------------------------------------*/

SymbolRecord * addBooleanConst(SymbolTable *symbolTable, char *name, int value) {
    SymbolRecord *varSymRec;
    varSymRec = addSymbol(symbolTable, name, SK_CONST, ST_BOOL, 0);
    varSymRec->constValue = value;
    varSymRec->hasValue = true;
    return varSymRec;
}

SymbolTable *initSymbolTable(char *name, bool isGlobalTable) {
    SymbolTable *newTable = malloc(sizeof(SymbolTable));
    newTable->name = name;
    newTable->firstSymbol = NULL;
    newTable->lastSymbol = NULL;
    newTable->parentTable = NULL;

    if (isGlobalTable) {
        // add built-in symbols
        addBooleanConst(newTable, "false", 0);
        addBooleanConst(newTable, "true", 1);
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

void setStructSize(SymbolRecord *symbol, int unionSize) {
    symbol->numElements = unionSize;
}

void markFunctionUsed(SymbolRecord *funcSymbol) {
    if (funcSymbol && funcSymbol->funcExt) {
        funcSymbol->funcExt->cntUses++;
    }
}

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

    if (isArray || isStruct) varSize = varSymRec->numElements;

    if ((varSymRec->flags & ST_MASK) == ST_INT || isPtr) varSize = varSize * 2;
    return varSize;
}

int getBaseVarSize(const SymbolRecord *varSymRec) {
    bool isPointer = (varSymRec->flags & MF_POINTER);
    bool isInt = ((varSymRec->flags & ST_MASK) == ST_INT);
    return (isPointer || isInt) ? 2 : 1;
}

int getCodeSize(const SymbolRecord *funcSymRec) {
    if (funcSymRec->funcExt == NULL) return 0;

    bool isFunc = isFunction(funcSymRec);
    bool isCodeUsed = (funcSymRec->funcExt->cntUses > 0);
    return (isFunc && isCodeUsed) ? funcSymRec->funcExt->instrBlock->codeSize : 0;
}

void addSymbolLocation(SymbolRecord *symbolRecord, int location) {
    symbolRecord->hasLocation = true;
    symbolRecord->location = location;
}

void addSymbolExt(SymbolRecord *funcSym, SymbolTable *paramTbl, SymbolTable *localSymTbl) {
    if (funcSym == NULL) return;

    SymbolExt *funcExt = malloc(sizeof(SymbolExt));
    funcExt->paramSymbolSet = paramTbl;
    funcExt->localSymbolSet = localSymTbl;
    funcExt->isInlined = false;
    funcExt->inlinedCode = NULL;
    funcExt->cntUses = 0;

    funcSym->funcExt = funcExt;
}


SymbolRecord * findSymbol(SymbolTable *symbolTable, const char* name) {
    SymbolRecord *curSymbol = symbolTable->firstSymbol;
    while (curSymbol && strncmp(curSymbol->name, name, SYMBOL_NAME_LIMIT) != 0) {
        curSymbol = curSymbol->next;
    }
    return curSymbol;
}

/**
 * isSimpleConst - is this a constant that has a singular value?
 * @param symbol
 * @return
 */
bool isSimpleConst(SymbolRecord *symbol) {
    unsigned int flags = symbol->flags;
    return (symbol->kind == SK_CONST) && !(flags & (MF_ARRAY | ST_STRUCT | SK_STRUCT));
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

bool isPointer(const SymbolRecord *symbol) {
    return (symbol->flags & MF_POINTER) != 0;
}

bool isArray(const SymbolRecord *symbol) {
    return (symbol->flags & MF_ARRAY) != 0;
}

bool isStruct(const SymbolRecord *symbol) {
    return (symbol->kind == SK_STRUCT);
}

bool isUnion(const SymbolRecord *symbol) {
    return (symbol->kind == SK_UNION);
}

bool isMainFunction(const SymbolRecord *symbol) {
    return (strcmp(symbol->name, "main")==0);
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
    char *symName = malloc(strlen(curSymbol->name) + indentSize + 1);
    if (indentLevel > 0) {
        for (int i=0; i<indentLevel*2; i++) symName[i] = ' ';
    }
    strcpy(symName + indentSize, curSymbol->name);

    char value[6] = "";
    if (curSymbol->hasValue) {
        sprintf(value, "%4x", curSymbol->constValue);
    }

    char location[6] = "";
    if (curSymbol->hasLocation) {
        sprintf(location, "@%04x", curSymbol->location);
    }

    char userTypeName[32] = "";
    if (curSymbol->userTypeDef != NULL) {
        sprintf(userTypeName, "%s", curSymbol->userTypeDef->name);
    }

    // print information
    bool isFunc = isFunction(curSymbol) && (curSymbol->funcExt != NULL);
    int baseSize = getBaseVarSize(curSymbol);
    int size = isFunc ? getCodeSize(curSymbol) : calcVarSize(curSymbol);
    //curSymbol->numElements * baseSize;
    fprintf(outputFile,
            " %-20s  %6s  %04x %5s  %5s  %02x  %02x  %04x  %6s  %20s\n",
            symName,
            lookupSymbolKind(curSymbol->kind),
            curSymbol->flags,
            ((curSymbol->flags & MF_POINTER) != 0) ? "true" : "false",
            location,
            baseSize,
            curSymbol->numElements,
            size,
            value,
            userTypeName);

    free(symName);
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
    if (curSymbol->funcExt == NULL) return;

    if ((curSymbol->kind == SK_STRUCT) ||
        (curSymbol->kind == SK_UNION)) {
        SymbolTable *structSymTbl = curSymbol->funcExt->paramSymbolSet;
        printSubSymbolTable(outputFile, structSymTbl, indentLevel + 1);
    } else if (isFunction(curSymbol)) {
        SymbolTable *paramSymTbl = curSymbol->funcExt->paramSymbolSet;
        if (paramSymTbl != NULL) {
            fprintf(outputFile, "  Params:\n");
            printSubSymbolTable(outputFile, paramSymTbl, indentLevel + 2);
        }
        SymbolTable *localSymTbl = curSymbol->funcExt->localSymbolSet;
        if (localSymTbl != NULL) {
            fprintf(outputFile, "  Locals:\n");
            printSubSymbolTable(outputFile, localSymTbl, indentLevel + 2);
        }
    }
}

void printSymbolHeader(FILE *outputFile) {
    fprintf(outputFile, "     Symbol Name        Kind  Flags  Pntr   Loc   BS  #El  Size  Value\n");
    fprintf(outputFile, "-----------------------------------------------------------------------\n");
}

void printSingleSymbol(FILE *outputFile, const SymbolRecord *curSymbol) {
    printSymbolHeader(outputFile);
    printSymbol(outputFile, curSymbol, 0);
    fprintf(outputFile, "\n");
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



//-----------------------------------------------------------------------
//   Internal functions


SymbolRecord * newSymbol(char *tname, enum SymbolKind kind, enum SymbolType type, unsigned int flags) {
    SymbolRecord *newSymbol = (SymbolRecord *)malloc(sizeof(SymbolRecord));

    strncpy(newSymbol->name, tname, SYMBOL_NAME_LIMIT);
    newSymbol->kind = kind;
    newSymbol->flags = flags | type;
    newSymbol->isLocal = false;
    newSymbol->hint = VH_NONE;
    newSymbol->hasLocation = false;
    newSymbol->location = 0xffffffff;
    newSymbol->hasValue = false;
    newSymbol->numElements = 0;
    newSymbol->funcExt = NULL;
    newSymbol->userTypeDef = NULL;
    newSymbol->next = NULL;
    newSymbol->constEvalNotes = "";

    return newSymbol;
}

void killSymbol(SymbolRecord *symbol) {
    printf("killed: %s\n", symbol->name);
    free(symbol);
}

char* lookupSymbolKind(enum SymbolKind symbolKind) {
    int i=0;
    do {
        if (SymbolKinds[i].symbolKindEnum == symbolKind)
            return SymbolKinds[i].kindName;
    } while (++i < NumSymbolKinds);
    return "";
}

void symbol_setAddr(SymbolRecord *sym, int funcAddr) {
    sym->location = funcAddr;
    sym->hasLocation = true;
}

bool isStructDefined(const SymbolRecord *structSymbol) {
    return (structSymbol && structSymbol->funcExt && structSymbol->funcExt->paramSymbolSet);
}

SymbolTable *getStructSymbolSet(const SymbolRecord *structSymbol) {
    return structSymbol->userTypeDef->funcExt->paramSymbolSet;
}

const char *getVarName(const SymbolRecord *varSym) {
    if (varSym->isLocal) {
        char *varName = malloc(strlen(varSym->name)+2);
        strcpy(varName, ".");
        return strcat(varName, varSym->name);
    } else {
        return varSym->name;
    }
}