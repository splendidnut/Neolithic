/**
 *        Symbol Table interface
 */

#ifndef MODULE_SYMBOL_TABLE
#define MODULE_SYMBOL_TABLE

#include <stdbool.h>
#include <stdio.h>
#include "syntax_tree.h"

#define SYMBOL_NAME_LIMIT 32

//---  Macros for getting basic information about a SymbolRecord

#define IS_ALIAS(sym) ((sym)->kind == SK_ALIAS)
#define IS_LOCAL(sym) (((sym)->flags & MF_LOCAL) != 0)

#define IS_STACK_VAR(sym) (((sym)->flags & SS_STORAGE_MASK) == SS_STACK)
#define IS_PARAM_VAR(sym) ((sym)->flags & MF_PARAM)

#define HAS_SYMBOL_LOCATION(sym)  ((sym)->location >= 0)

//--- macros for function symbols
#define GET_LOCAL_SYMBOL_TABLE(funcSym) ((funcSym)->symbolTbl)
#define GET_FUNCTION_DEPTH(funcSym)     ((funcSym)->funcDepth)
#define IS_FUNC_USED(funcSym)           ((funcSym)->cntUses > 0)
#define IS_INLINED_FUNCTION(funcSym)    ((funcSym)->flags & MF_INLINE)

#define GET_STRUCT_SYMBOL_TABLE(structSym)  ((structSym)->symbolTbl)
#define getStructSymbolSet(sym)             GET_STRUCT_SYMBOL_TABLE((sym)->userTypeDef)
#define GET_PROPERTY_OFFSET(propertySymbol) ((propertySymbol)->location)

//--------------------------------------
//   All the flags for symbols

enum SymbolType {           //-- base type of variable
    ST_NONE,
    ST_CHAR = 0x01,
    ST_INT  = 0x02,
    ST_BOOL = 0x03,

    ST_STRUCT = 0x04,     //-- uses a structure

    ST_PTR = 0x05,        //-- pointer (used in code generator)

    ST_MASK = 0x07,

    ST_UNSIGNED = 0x00,     //-- default
    ST_SIGNED = 0x08,

    ST_ERROR = 0x0F     //--- ERROR when used as destination type
};
enum SymbolKind {
    SK_NONE,
    SK_VAR    = 0x01,
    SK_CONST  = 0x02,
    SK_FUNC   = 0x03,

    // user-defined type definitions
    SK_STRUCT = 0x04,
    SK_UNION  = 0x05,
    SK_ENUM   = 0x06,

    SK_ALIAS  = 0x08,

    SK_MASK   = 0x0F
};
enum ModifierFlags {
    MF_NONE,

    // flags for storage location
    SS_NONE         = 0x0000,
    SS_ABSOLUTE     = 0x0010,           // ABSOLUTE = non-zeropage (default)
    SS_ZEROPAGE     = 0x0020,           // ZEROPAGE = stored in zeropage (6502)
    SS_REGISTER     = 0x0030,           // REGISTER = attempt to stick in register (A,X,Y,?)
    SS_STACK        = 0x0040,
    SS_ROM          = 0x0050,
    SS_STORAGE_MASK = 0x0070,

    // flags for scope --- Since we're potentially combining these scopes into the same (sub-) symbol table.
    MF_PARAM        = 0x0100,
    MF_LOCAL        = 0x0200,

    MF_ENUM_VALUE   = 0x1000,     // only used in enumeration definitions (TODO: is this necessary?)
    MF_INLINE       = 0x2000,       // only applies to functions

    // these are the two important flags, so I put them at the very top of the bitspace
    MF_ARRAY        = 0x4000,
    MF_POINTER      = 0x8000
};

enum VarHint {
    VH_NONE,
    VH_A_REG,
    VH_X_REG,
    VH_Y_REG,
};


typedef struct SymbolRecordStruct {     // 112 bytes!
    struct SymbolRecordStruct *next;        // point to next symbol

    //--- definition
    char *name;
    enum SymbolKind kind;
    unsigned int flags;                     // ModifierFlags + SymbolType
    int numElements;                        // number of elements (if array/object)
    int location;                           // location in memory  (has location if >= 0)

    struct SymbolTableStruct *symbolTbl;    // symbol table set if function or struct symbol
    struct SymbolRecordStruct *userTypeDef; // user defined type
    List *astList;                          // code / data list from AST

    //---- All the rest of these are SymbolKind specific

    // used by SK_CONST - constant value information
    bool hasValue;
    int constValue;                     // constant value
    char *constEvalNotes;

    // used by SK_ALIAS
    List *alias;

    // used by SK_VAR - function params
    enum VarHint hint;                  // Hint for Function Param Symbol

    // used by SK_FUNC
    int cntUses;                            // number of times function is used
    int funcDepth;                          // track the depth of the function
    struct InstrBlockStruct *instrBlock;

} SymbolRecord;


/**
 * Structure to store a symbol table.
 *
 * Uses a linked list with pointers for the first and last entries
 *  (allows fast additions)
 */
typedef struct SymbolTableStruct {
    SymbolRecord *firstSymbol;
    SymbolRecord *lastSymbol;
    char *name;

    struct SymbolTableStruct *parentTable;
} SymbolTable;

/**
 * Structure to hold a small symbol list (used mainly for function parameters)
 */

#define MAX_SYMBOL_LIST_CNT 10
typedef struct stSymbolList {
    int count;
    int cntStackVars;
    SymbolRecord *list[MAX_SYMBOL_LIST_CNT];
} SymbolList;


extern char* getNameOfSymbolKind(enum SymbolKind symbolKind);
extern enum SymbolType getSymbolType(const char *baseType);

extern SymbolRecord * addConst(SymbolTable *symbolTable, char *name, int value, enum SymbolType type, enum ModifierFlags flags);

extern SymbolTable *initSymbolTable(char *name, SymbolTable *parentTable);
extern SymbolRecord * addSymbol(SymbolTable *, char *name, enum SymbolKind kind, enum SymbolType type, unsigned int flags);

extern void setSymbolLocation(SymbolRecord *symbolRecord, int location, enum ModifierFlags storageType);
extern void setSymbolArraySize(SymbolRecord *symbol, int arraySize);
extern void setStructSize(SymbolRecord *symbol, int unionSize);
extern unsigned char getStructVarSize(const SymbolRecord *symbol);
extern void markFunctionUsed(SymbolRecord *funcSymbol);
extern int calcVarSize(const SymbolRecord *varSymRec);
extern int getBaseVarSize(const SymbolRecord *varSymRec);
extern SymbolRecord * findSymbol(SymbolTable *symbolTable, const char *name);
extern char getDestRegFromHint(enum VarHint hint);
extern SymbolList *getParamSymbols(SymbolTable *symTblWithParams);
extern SymbolRecord *lookupProperty(SymbolRecord *structSymbol, char *propName);

//--------------------------------------------------------
//--- TODO:  convert some/most of these to simple macros?

extern bool isSimpleConst(SymbolRecord *symbol);
extern bool isArrayConst(SymbolRecord *symbol);
extern bool isConst(const SymbolRecord *symbol);
extern bool isVariable(const SymbolRecord *curSymbol);
extern bool isFunction(const SymbolRecord *symbol);
extern bool isStruct(const SymbolRecord *symbol);
extern bool isUnion(const SymbolRecord *symbol);
extern bool isEnum(const SymbolRecord *symbol);
extern bool isPointer(const SymbolRecord *symbol);
extern bool isArray(const SymbolRecord *symbol);
extern bool isMainFunction(const SymbolRecord *symbol);

//------

extern enum SymbolType getType(const SymbolRecord *symbol);
extern void showSymbolTable(FILE *outputFile, SymbolTable *symbolTable);
extern void killSymbolTable(SymbolTable *symbolTable);

extern bool isStructDefined(const SymbolRecord *structSymbol);
extern const char *getVarName(const SymbolRecord *varSym);


// USE ONLY FOR DEBUGGING!
extern void printSingleSymbol(FILE *outputFile, const SymbolRecord *curSymbol);

#endif
