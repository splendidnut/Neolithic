//
// Created by admin on 6/14/2021.
//

#include <stdio.h>

#include "gen_alloc.h"
#include "data/func_map.h"

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

int allocateVarStorage(const SymbolTable *symbolTable, int curMemloc) {
    SymbolRecord *curSymbol = symbolTable->firstSymbol;
    while (curSymbol != NULL) {

        // only vars need allocation storage
        if (isVariable(curSymbol) && (curSymbol->location != 0xffff)) {
            int varSize = calcVarSize(curSymbol);

            // allocate memory
            MemoryArea *memoryArea = (curSymbol->flags & MF_ZEROPAGE) ? SMA_getZeropageArea() : NULL;

            MemoryAllocation newVarAlloc = SMA_allocateMemory(memoryArea, varSize);
            printf("Variable %s allocated at %4X / %4X\n", curSymbol->name, newVarAlloc.addr, curMemloc);

            addSymbolLocation(curSymbol, curMemloc);

            // move mem allocation pointer to next spot
            curMemloc += varSize;
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
                allocateVarStorage(funcSymTbl, startMemLoc);
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
    printf("Calculate Local Variable allocations\n");
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


                int localMemNeeded = calcStorageNeeded(funcSymTbl);
                curSymbol->funcExt->localVarMemUsed = localMemNeeded;
                printf("  Func %s (depth %d, calls %d funcs) needs %d bytes for locals\n",
                       curSymbol->name,
                       curSymbol->funcExt->funcDepth,
                       funcsCalled,
                       localMemNeeded);
            }
        }
        curSymbol = curSymbol->next;
    }
}

void generate_var_allocations(SymbolTable *symbolTable, MemoryArea *varStorage) {
    printf("Allocating memory for variables\n");

    int curMemloc = varStorage->startAddr;
    curMemloc = allocateVarStorage(symbolTable, curMemloc);

    // TODO: local vars should be allocated based on function depth!
    calcLocalVarAllocs(symbolTable);
    allocateLocalVars(symbolTable, curMemloc);
}