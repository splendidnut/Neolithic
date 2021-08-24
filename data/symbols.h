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
    //--- definition
    char name[SYMBOL_NAME_LIMIT];
    enum SymbolKind kind;
    unsigned int flags;                 // ModifierFlags + SymbolType
    int numElements;                  // number of elements (if array/object)
    int location;                       // location in memory  (hasLocation if >= 0)

    //---- All the rest of these are SymbolKind specific

    // constant value information
    bool hasValue;
    int constValue;                     // constant value
    char *constEvalNotes;

    // used by SK_ALIAS
    List *alias;

    // used by function params
    enum VarHint hint;                  // Hint for Function Param Symbol

    struct SymbolExtStruct *funcExt;
    struct SymbolRecordStruct *userTypeDef;     // user defined type
    struct SymbolRecordStruct *next;  // point to next symbol
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
 * Extension structure for Symbols
 *   - stores size of generated code (for functions)
 *   - stores parameter symbol table (Functions / Structures)
 *   - stores local symbol table (for functions)
 */
typedef struct SymbolExtStruct {
    int cntUses;        // number of times function is used
    int funcDepth;
    int cntParams;
    int localVarMemUsed;   // local variable memory required
    bool isInlined;
    struct ListStruct *inlinedCode; // source of function code (for use with inlining)
    struct InstrStruct *instrList;  // compiled function code as instruction list
    struct InstrBlockStruct *instrBlock;

    struct SymbolTableStruct *paramSymbolSet;
    struct SymbolTableStruct *localSymbolSet;
} SymbolExt;

extern char *getSymbolKindName(enum SymbolKind symbolKind);
extern enum SymbolType getSymbolType(const char *baseType);

extern SymbolTable *initSymbolTable(char *name, bool isGlobalTable);
extern SymbolRecord * addSymbol(SymbolTable *, char *name, enum SymbolKind kind, enum SymbolType type, unsigned int flags);

extern void setSymbolLocation(SymbolRecord *symbolRecord, int location, enum ModifierFlags storageType);
extern void setSymbolArraySize(SymbolRecord *symbol, int arraySize);
extern void setStructSize(SymbolRecord *symbol, int unionSize);
extern void markFunctionUsed(SymbolRecord *funcSymbol);
extern int calcVarSize(const SymbolRecord *varSymRec);
extern int getBaseVarSize(const SymbolRecord *varSymRec);
extern SymbolRecord * findSymbol(SymbolTable *symbolTable, const char *name);

//--------------------------------------------------------
//--- TODO:  convert some/most of these to simple macros?

extern bool isSimpleConst(SymbolRecord *symbol);
extern bool isArrayConst(SymbolRecord *symbol);
extern bool isConst(const SymbolRecord *symbol);
extern bool isVariable(const SymbolRecord *curSymbol);
extern bool isFunction(const SymbolRecord *symbol);
extern bool isStruct(const SymbolRecord *symbol);
extern bool isUnion(const SymbolRecord *symbol);
extern bool isPointer(const SymbolRecord *symbol);
extern bool isArray(const SymbolRecord *symbol);
extern bool isMainFunction(const SymbolRecord *symbol);

//------

extern enum SymbolType getType(const SymbolRecord *symbol);
extern void addSymbolExt(SymbolRecord *funcSym, SymbolTable *paramTbl, SymbolTable *localSymTbl);
extern void showSymbolTable(FILE *outputFile, SymbolTable *symbolTable);
extern void killSymbolTable(SymbolTable *symbolTable);

extern bool isStructDefined(const SymbolRecord *structSymbol);
extern SymbolTable *getStructSymbolSet(const SymbolRecord *structSymbol);
extern const char *getVarName(const SymbolRecord *varSym);


// USE ONLY FOR DEBUGGING!
extern void printSingleSymbol(FILE *outputFile, const SymbolRecord *curSymbol);

#endif
