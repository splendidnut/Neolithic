//
// Created by admin on 6/14/2021.
//

#include <stdio.h>

#include "gen_alloc.h"
#include "data/func_map.h"

#define DEBUG_ALLOCATOR

//-------------------------------
// Module variables

static int stackSizes[8];       // Track the sizes of 8 stack frames
static int stackLocs[8];        // Location of each stack frame


void initStackFrames() {
    for_range(i, 0, 7) {
        stackSizes[i] = 0;
        stackLocs[i] = -1;
    }
}

/**
 * Calculate storage necessary for specified symbol table
 * @param symbolTable
 * @return
 */
int calcStorageNeeded(const SymbolTable *symbolTable) {
    int sizeNeeded = 0;
    SymbolRecord *curSymbol = symbolTable->firstSymbol;
    while (curSymbol != NULL) {
        if (isVariable(curSymbol) && (curSymbol->location != 0xffff)) {
            int varSize = calcVarSize(curSymbol);
            sizeNeeded += varSize;
        }
        curSymbol = curSymbol->next;
    }
    return sizeNeeded;
}

/**
 * Allocate variable storage using the Static Memory Allocator
 *
 * @param symbolTable
 * @param curMemloc
 * @return
 */
int allocateVarStorage(const SymbolTable *symbolTable, int curMemloc) {
    printf("\nVariables for %s\n", (symbolTable->name != NULL ? symbolTable->name : "(none)"));
    SymbolRecord *curSymbol = symbolTable->firstSymbol;
    while (curSymbol != NULL) {

        // only vars need allocation storage
        if (isVariable(curSymbol) && (curSymbol->location != 0xffff)) {
            int varSize = calcVarSize(curSymbol);

            // allocate memory
            MemoryArea *memoryArea = (curSymbol->flags & MF_ZEROPAGE) ? SMA_getZeropageArea() : NULL;

            MemoryAllocation newVarAlloc = SMA_allocateMemory(memoryArea, varSize);
            printf("\t%s allocated at %4X / %4X\n", curSymbol->name, newVarAlloc.addr, curMemloc);

            addSymbolLocation(curSymbol, curMemloc);

            // move mem allocation pointer to next spot
            curMemloc += varSize;
        }
        curSymbol = curSymbol->next;
    }
    return curMemloc;
}

/**
 * Allocate storage needed for each stack frame
 *
 * Currently stacks are allocated with non-overlapping memory
 *
 * TODO:
 *    There's the potential for optimization of the stack space
 *    usage if we track more than one stack frame per level.
 *    Tracking two frames per stack frame level:
 *      Functions that are end nodes (can use rest of stack space)
 *      Functions that call other functions (cannot overlap)
 */
void allocateStackFrameStorage() {
    printf("\nStack Frames:\n");
    for_range(frmNum, 0, 7) if (stackSizes[frmNum] > 0) {
        MemoryAllocation newVarAlloc = SMA_allocateMemory(0, stackSizes[frmNum]);
        stackLocs[frmNum] = newVarAlloc.addr;
        printf("\tFrame %d allocated %2d bytes at %4X\n", frmNum, stackSizes[frmNum], newVarAlloc.addr);
    }
}

/**
 * Allocate local variable using the provided memory location,
 *  then return the next available memory location
 *
 * @param symbolTable
 * @param curMemloc
 * @return
 */
int allocateLocalVarStorage(const SymbolTable *symbolTable, int curMemloc) {
    printf("\nVariables for %s\n", (symbolTable->name != NULL ? symbolTable->name : "(none)"));
    SymbolRecord *curSymbol = symbolTable->firstSymbol;
    while (curSymbol != NULL) {

        // only vars need allocation storage
        if (isVariable(curSymbol) && (curSymbol->location != 0xffff)) {
            int startLoc = curMemloc;
            addSymbolLocation(curSymbol, curMemloc);
            int varSize = calcVarSize(curSymbol);
            curMemloc += varSize;

            printf("\t%-20s allocated at %4X\n", curSymbol->name, startLoc);
        }
        curSymbol = curSymbol->next;
    }
    return curMemloc;
}

// Walk thru functions and assign memory locations to local vars
void allocateLocalVars(const SymbolTable *symbolTable, int startMemLoc) {
    SymbolRecord *curSymbol = symbolTable->firstSymbol;
    while (curSymbol != NULL) {
        if (isFunction(curSymbol)) {
            SymbolTable *funcSymTbl = curSymbol->funcExt->localSymbolSet;
            if (funcSymTbl != NULL) {
                // ****** TODO: Remove after finished with new version
                //allocateVarStorage(funcSymTbl, startMemLoc);

                int funcDepth = curSymbol->funcExt->funcDepth;
                allocateLocalVarStorage(funcSymTbl, stackLocs[funcDepth]);
            }
        }
        curSymbol = curSymbol->next;
    }
}

/**
 * Calculate local variable allocations
 *
 * Do this so we can figure out where we will allocate this information in ZeroPage memory
 *
 * @param symbolTable
 */
void calcLocalVarAllocs(const SymbolTable *symbolTable) {
    printf("\nCalculate Local Variable allocations\n");

    // Walk thru all symbols in the symbol table
    SymbolRecord *curSymbol = symbolTable->firstSymbol;
    while (curSymbol != NULL) {
        if (isFunction(curSymbol)) {
            SymbolTable *funcSymTbl = curSymbol->funcExt->localSymbolSet;
            if (funcSymTbl != NULL) {

                // figure out if this function calls other functions
                int funcsCalled = 0;
                FuncCallMapEntry *funcCallMapEntry = FM_findFunction(curSymbol->name);
                if (funcCallMapEntry != NULL) {
                    funcsCalled = funcCallMapEntry->cntFuncsCalled;
                }

                // calculate local memory needed by function
                int localMemNeeded = calcStorageNeeded(funcSymTbl);
                curSymbol->funcExt->localVarMemUsed = localMemNeeded;

                // keep track of each stack size
                int depth = curSymbol->funcExt->funcDepth;
                if (stackSizes[depth] < localMemNeeded) stackSizes[depth] = localMemNeeded;

                    //  DEBUG
#ifdef DEBUG_ALLOCATOR
                printf("  Func %s (depth %d, calls %d funcs) needs %d bytes for locals\n",
                       curSymbol->name,
                       curSymbol->funcExt->funcDepth,
                       funcsCalled,
                       localMemNeeded);
#endif
            }
        }
        curSymbol = curSymbol->next;
    }
}

void generate_var_allocations(SymbolTable *symbolTable, MemoryArea *varStorage) {
    printf("Allocating memory for variables\n");

    int curMemloc = varStorage->startAddr;
    curMemloc = allocateVarStorage(symbolTable, curMemloc);

    // now process all function local variables
    initStackFrames();
    calcLocalVarAllocs(symbolTable);
    allocateStackFrameStorage();
    allocateLocalVars(symbolTable, curMemloc);
}