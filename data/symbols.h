/**
 *        Symbol Table interface
*/

#ifndef MODULE_SYMBOL_TABLE
#define MODULE_SYMBOL_TABLE

#include <stdbool.h>
#include <stdio.h>

#define SYMBOL_NAME_LIMIT 32

//--------------------------------------
//   All the flags for symbols

enum SymbolType {           //-- base type of variable
    ST_NONE,
    ST_CHAR = 0x01,
    ST_INT  = 0x02,
    ST_BOOL = 0x03,

    ST_STRUCT = 0x04,     //-- uses a structure

    ST_PTR = 0x05,        //-- pointer (used in code generator)

    ST_USER = 0x06,
    ST_MASK = 0x07,

    ST_UNSIGNED = 0x00,     //-- default
    ST_SIGNED = 0x08,

    ST_ERROR = 0x0F     //--- ERROR when used as destination type
};
enum SymbolKind {
    SK_NONE,            //-- used for labels
    SK_VAR    = 0x10,
    SK_CONST  = 0x20,
    SK_FUNC   = 0x30,

    SK_STRUCT = 0x40,
    SK_UNION  = 0x50,
    SK_ENUM   = 0x60,

    SK_MASK   = 0x70
};
enum ModifierFlags {
    MF_NONE,
    MF_PARAM    = 0x0080,
    MF_INLINE   = 0x0100,

    MF_ABSOLUTE     = 0x0000,
    MF_ZEROPAGE     = 0x0400,
    MF_REGISTER     = 0x0800,
    MF_STORAGE_MASK = 0x0C00,

    MF_ENUM_VALUE = 0x1000,
    MF_ARRAY    = 0x4000,
    MF_POINTER  = 0x8000
};
enum SymbolStorage {
    SS_ABSOLUTE = 0x0,
    SS_ZEROPAGE = 0x1,
    SS_STACK    = 0x2,
    SS_REGISTER = 0x3,
    SS_ROM      = 0x4
};

enum VarHint {
    VH_NONE,
    VH_A_REG,
    VH_X_REG,
    VH_Y_REG,
};

/**
 *  STATIC   = only available in source (file or function), sticks around
 *  EXTERN   = defined somewhere else
 *
 * ------------------------------------
 * Storage locations:
 *
 *  ABSOLUTE = non-zeropage (default)
 *  ZEROPAGE = stored in zeropage (6502)
 *  REGISTER = attempt to stick in register (A,X,Y,?)
 *  PARAM    = stored on stack... or register (passed into function)
 *
 *  -----------------------------------
 *  flags:
 *
 *  SIGNED  - is value signed?
 *  CONST   - cannot be changed (generally stored in ROM)
 *  ARRAY   - flag for array
 *  POINTER - flag for indirection
 *
 *  -----------------------------------
 *  Symbol Categories (Symbol Kind):
 *     VARIABLE
 *     FUNCTION
 *     TYPE
 */

typedef struct SymbolRecordStruct {
    char name[SYMBOL_NAME_LIMIT];
    enum SymbolKind kind;
    unsigned int flags;                 // ModifierFlags + SymbolType + SymbolKind

    bool hasLocation;
    int location;                       // location in memory
    bool hasValue;
    int constValue;                     // constant value
    char *constEvalNotes;
    int numElements;                  // number of elements (if array/object)

    // used by function params
    enum VarHint hint;                  // Hint for Function Param Symbol
    bool isLocal;
    bool isStack;                       // Function Parma: Is On Stack?

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

extern SymbolTable *initSymbolTable(bool isGlobalTable);
extern SymbolRecord * addSymbol(SymbolTable *, char *name, enum SymbolKind kind, enum SymbolType type, unsigned int flags);
extern void setSymbolArraySize(SymbolRecord *symbol, int arraySize);
extern void setStructSize(SymbolRecord *symbol, int unionSize);
extern void markFunctionUsed(SymbolRecord *funcSymbol);
extern int calcVarSize(const SymbolRecord *varSymRec);
extern int getBaseVarSize(const SymbolRecord *varSymRec);
extern void addSymbolLocation(SymbolRecord *symbolRecord, int location);
extern SymbolRecord * findSymbol(SymbolTable *symbolTable, const char *name);

extern bool isSimpleConst(SymbolRecord *symbol);
extern bool isArrayConst(SymbolRecord *symbol);
extern bool isConst(const SymbolRecord *symbol);
extern bool isVariable(const SymbolRecord *curSymbol);
extern bool isFunction(const SymbolRecord *symbol);
extern bool isPointer(const SymbolRecord *symbol);
extern bool isArray(const SymbolRecord *symbol);
extern bool isStruct(const SymbolRecord *symbol);
extern bool isUnion(const SymbolRecord *symbol);
extern bool isMainFunction(const SymbolRecord *symbol);

extern enum SymbolType getType(const SymbolRecord *symbol);
extern void addSymbolExt(SymbolRecord *funcSym, SymbolTable *paramTbl, SymbolTable *localSymTbl);
extern void showSymbolTable(FILE *outputFile, SymbolTable *symbolTable);
extern void killSymbolTable(SymbolTable *symbolTable);

extern void symbol_setAddr(SymbolRecord *sym, int funcAddr);
extern bool isStructDefined(const SymbolRecord *structSymbol);
extern SymbolTable *getStructSymbolSet(const SymbolRecord *structSymbol);
extern const char *getVarName(const SymbolRecord *varSym);


// USE ONLY FOR DEBUGGING!
extern void printSingleSymbol(FILE *outputFile, const SymbolRecord *curSymbol);

#endif
