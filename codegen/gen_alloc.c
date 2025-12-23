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

//
//  Module for Generating Allocations for Variables (both global and local)
//
//  For local vars:
//      Generates statically-allocated stack frames for use as storage for the local variables of each function
//
// Created by admin on 6/14/2021.
//
/*
 * TODO - Currently in progress:
 *    There's the potential for optimization of the stack space
 *    usage if we track more than one stack frame per level.
 *    Tracking two frames per stack frame level:
 *      Functions that are end nodes (can use rest of stack space)
 *      Functions that call other functions (cannot overlap)
 */

#include <stdio.h>
#include <stdlib.h>

#include "gen_alloc.h"
#include "data/func_map.h"
#include "gen_common.h"

#define DEBUG_ALLOCATOR

#define MAX_STACK_FRAMES 8

//-------------------------------
// Module variables

static int globalSize;
static int globalAddr;
static int stackSizes[MAX_STACK_FRAMES];       // Track the sizes of 8 stack frames
static int stackLocs[MAX_STACK_FRAMES];        // Location of each stack frame
static int callStackDepth;


void initStackFrames() {
    for_range(i, 0, MAX_STACK_FRAMES-1) {
        stackSizes[i] = 0;
        stackLocs[i] = -1;
    }
    callStackDepth = 0;
}

// Need to collect list of functions in order of depth
//  starting with the deepest
//
//  This is done using bucket lists for each depth
//
//  TODO:  Potentially should fix the Function Map module to do this instead.

typedef struct {
    int depth;
    SymbolRecord *symbol;
} DepthSymbolRecord;

DepthSymbolRecord depthSymbolList[100];
int cntDepthSymbols;

int cmp_depths(const void * arg1, const void * arg2) {
    int d1 = ((DepthSymbolRecord *) arg1)->depth;
    int d2 = ((DepthSymbolRecord *) arg2)->depth;
    return (d2 - d1);   // DESCENDING
}

void collectFunctionsInOrder(const SymbolTable *symbolTable) {
    cntDepthSymbols = 0;

    // collect all functions that have local variables
    SymbolRecord *curSymbol = symbolTable->firstSymbol;
    while (curSymbol != NULL) {
        if (isFunction(curSymbol)) {        // Only need function symbols
            SymbolTable *funcSymTbl = GET_LOCAL_SYMBOL_TABLE(curSymbol);

            // Only need functions with local variables -- TODO: this might be a bad assumption
            if (funcSymTbl != NULL) {
                int depth = GET_FUNCTION_DEPTH(curSymbol);
                depthSymbolList[cntDepthSymbols].depth = depth;
                depthSymbolList[cntDepthSymbols].symbol = curSymbol;
                cntDepthSymbols++;
            }
        }
        curSymbol = curSymbol->next;
    }

    //---- now need to sort the list by depth
    qsort(depthSymbolList, cntDepthSymbols, sizeof(DepthSymbolRecord), cmp_depths);

    //--- now print the list
    if (compilerOptions.showCallTree) {
        printf("Collected functions (sorted by depth):\n");
        for (int idx = 0; idx < cntDepthSymbols; idx++) {
            printf("  %d %s\n", depthSymbolList[idx].depth, depthSymbolList[idx].symbol->name);
        }
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

            if (!IS_PARAM_VAR(curSymbol)) {
                int varSize = calcVarSize(curSymbol);
                sizeNeeded += varSize;
            }
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
int allocateVarStorage(const SymbolTable *symbolTable) {
    if (compilerOptions.showVarAllocations) {
        printf("\nVariables for %s\n", (symbolTable->name != NULL ? symbolTable->name : "(none)"));
    }

    int totalSize = 0;
    SymbolRecord *curSymbol = symbolTable->firstSymbol;
    while (curSymbol != NULL) {

        bool hasHint = ((curSymbol->flags & MF_HINT) != 0) && (curSymbol->location >= 0);

        // only vars need allocation storage (and only when they don't already have a location assigned via hint)
        if (isVariable(curSymbol) && (curSymbol->location != 0xffff) && (!hasHint)) {
            int varSize = calcVarSize(curSymbol);

            // allocate memory
            bool isZP = ((curSymbol->flags & SS_STORAGE_MASK) == SS_ZEROPAGE);
            MemoryArea *memoryArea = isZP ? SMA_getZeropageArea() : NULL;
            MemoryAllocation newVarAlloc = SMA_allocateMemory(memoryArea, varSize);

            //--- if variable gets allocated on the zeropage, mark it as so.
            if (newVarAlloc.addr < 256) isZP = true;

            setSymbolLocation(curSymbol, newVarAlloc.addr, isZP ? SS_ZEROPAGE : SS_ABSOLUTE);

            if (compilerOptions.showVarAllocations) {
                printf("\t%-32s allocated at %4X\n", curSymbol->name, newVarAlloc.addr);
            }

            totalSize += varSize;
        }
        curSymbol = curSymbol->next;
    }
    return totalSize;
}

/**
 * Allocate storage needed for each stack frame
 *
 * Currently stacks are allocated with non-overlapping memory
 *
 */
void allocateStackFrameStorage() {
    if (compilerOptions.showVarAllocations) {
        printf("\nStack Frames:\n");
    }
    for_range(frmNum, 0, MAX_STACK_FRAMES-1) if (stackSizes[frmNum] > 0) {
        MemoryAllocation newVarAlloc = SMA_allocateMemory(0, stackSizes[frmNum]);
        stackLocs[frmNum] = newVarAlloc.addr;
        if (compilerOptions.showVarAllocations) {
            printf("\tFrame %d allocated %2d bytes at %4X\n", frmNum, stackSizes[frmNum], newVarAlloc.addr);
        }
    }

    ///---  Handle any stacks that didn't get allocated.
    // -- This code is a work-in-progress.  The code tries to handle situations
    //    where a stack frame didn't get allocated for some reason.

    for (int stkIdx=1; stkIdx < MAX_STACK_FRAMES-1; stkIdx++) {
        if ((stackSizes[stkIdx] == 0) && (stackSizes[stkIdx+1] > 0)) {
            stackLocs[stkIdx] = stackLocs[stkIdx+1];
            if (compilerOptions.showVarAllocations) {
                printf("\tFrame %d allocated (shared) at %4X\n",
                       stkIdx, stackLocs[stkIdx]);
            }
        }
    }
}

void showVariableAllocations() {
    printf("\nSummary of Variable Allocation:\n");
    printf("\tGlobal Frame   allocated %2d bytes at %4X\n", globalSize, globalAddr);

    int stackAddr = globalAddr + globalSize;
    for_range(frmNum, 1, MAX_STACK_FRAMES-1) if (stackSizes[frmNum] > 0) {
        printf("\t Local Frame %d allocated %2d bytes at %4X\n", frmNum, stackSizes[frmNum], stackLocs[frmNum]);
        stackAddr = stackSizes[frmNum] + stackLocs[frmNum];
    }

    int callStackUsage = callStackDepth * 2;
    int callStackAddr = 0x100 - callStackUsage;
    printf("\t  Call Stack   allocated %2d bytes at %4X (for %d call depth)\n", callStackUsage, callStackAddr, callStackDepth);

    // TODO: This is currently Atari 2600 specific (where Stack and Zero Page share memory)
    int remainingBytes = 0x100 - stackAddr - callStackUsage;
    //printf("\t Stack Frame   has       %2d bytes at %4X\n", 0x100-stackAddr, stackAddr);
    printf("\n\t\t%d bytes remaining\n", remainingBytes);
    printf("\n");
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
    if (compilerOptions.showVarAllocations) {
        printf("\nVariables for %s\n", (symbolTable->name != NULL ? symbolTable->name : "(none)"));
    }

    curFuncSymbolTable = (SymbolTable *)symbolTable;   //--- Needed for allowing symbol lookup (used by alias)
    SymbolRecord *curSymbol = symbolTable->firstSymbol;
    while (curSymbol != NULL) {

        // only vars need allocation storage
        if (isVariable(curSymbol) && (curSymbol->location != 0xffff)) {
            int startLoc = curMemloc;

            unsigned int curStorage = (curSymbol->flags & SS_STORAGE_MASK);
            if ((curStorage != SS_STACK) && (curSymbol->hint == VH_NONE)) {
                setSymbolLocation(curSymbol, curMemloc, SS_ZEROPAGE);       // TODO:  Locals are always stored in ZP (?)
                int varSize = calcVarSize(curSymbol);
                curMemloc += varSize;

            }
            if (compilerOptions.showVarAllocations) {
                if (curSymbol->hint > VH_NONE) {
                    printf("\t%-20s passed in register %c\n", curSymbol->name, getDestRegFromHint(curSymbol->hint));
                } else if (curStorage != SS_STACK) {
                    printf("\t%-20s allocated at %4X\n", curSymbol->name, startLoc);
                } else {
                    printf("\t%-20s allocated on stack\n", curSymbol->name);
                }
            }
        } else if (IS_ALIAS(curSymbol)) {

            // Need to assign memory location for simple alias variable (name alias)
            List *aliasDef = curSymbol->alias;
            if (aliasDef != NULL) {
                if ((aliasDef->count == 2) && (aliasDef->nodes[1].type == N_STR)) {
                    SymbolRecord *linkedVarSym = lookupSymbolNode(aliasDef->nodes[1], aliasDef->lineNum);
                    setSymbolLocation(curSymbol, linkedVarSym->location, linkedVarSym->flags & SS_STORAGE_MASK);
                    if (compilerOptions.showVarAllocations) {
                        printf("\t%-20s allocated at %4X - shared with %s\n",
                               curSymbol->name, linkedVarSym->location, linkedVarSym->name);
                    }
                }
            }
        }
        curSymbol = curSymbol->next;
    }
    return curMemloc;
}

// Walk thru functions and assign memory locations to local vars
void allocateLocalVars() {
    for_range (idx, 0, cntDepthSymbols) {
        DepthSymbolRecord curFuncSymbol = depthSymbolList[idx];
        if ((curFuncSymbol.symbol->funcDepth > 0) || isMainFunction(curFuncSymbol.symbol)) {
            SymbolTable *funcSymTbl = GET_LOCAL_SYMBOL_TABLE(curFuncSymbol.symbol);
            allocateLocalVarStorage(funcSymTbl, stackLocs[curFuncSymbol.depth]);
        } else if (compilerOptions.showVarAllocations) {
            printf("\nNo variables necessary for %s - function is unused\n", curFuncSymbol.symbol->name);
        }
    }
}


/**
 * Calculate local variable allocations
 *
 * Do this so we can figure out where we will allocate this information in ZeroPage memory
 *
 */
void calcLocalVarAllocs() {
    if (compilerOptions.showVarAllocations) {
        printf("\nCalculate Local Variable allocations\n");
    }

    // Walk thru all functions needing local frame for local vars
    int lastDepth = 20;
    int lastStackSize = 0;
    for_range (idx, 0, cntDepthSymbols) {
        SymbolRecord *curSymbol = depthSymbolList[idx].symbol;
        SymbolTable *funcSymTbl = GET_LOCAL_SYMBOL_TABLE(curSymbol);
        int depth = GET_FUNCTION_DEPTH(curSymbol);

        if (depth < lastDepth) {
            if (depth > callStackDepth) callStackDepth = depth;
            lastDepth = depth;
            lastStackSize += stackSizes[depth+1];
            if (compilerOptions.showVarAllocations) {
                printf(" @Depth: %d  -- Current Stack Size %d\n", depth, lastStackSize);
            }
        }

        // figure out if this function calls other functions
        int funcsCalled = 0;
        FuncCallMapEntry *funcCallMapEntry = FM_findFunction(curSymbol->name);
        if (funcCallMapEntry != NULL) {
            funcsCalled = funcCallMapEntry->cntFuncsCalled;
        }

        // calculate local memory needed by function
        int localMemNeeded = calcStorageNeeded(funcSymTbl);

        // keep track of each stack size
        if (funcsCalled == 0) {
            // if no functions are called, we can also use the deeper stack frames for vars
            if (stackSizes[depth]+lastStackSize  < localMemNeeded) {
                stackSizes[depth] = localMemNeeded - lastStackSize;
            }
        } else {
            if (stackSizes[depth] < localMemNeeded) {
                stackSizes[depth] = localMemNeeded;
            }
        }

        //  DEBUG
        if (compilerOptions.showVarAllocations) {
#ifdef DEBUG_ALLOCATOR
            printf("  Func %-20s (calls %d funcs) needs %d bytes for locals\n",
                   curSymbol->name,
                   funcsCalled,
                   localMemNeeded);
#endif
        }
    }
}

void generate_var_allocations(SymbolTable *symbolTable) {
    collectFunctionsInOrder(symbolTable);

    // allocate global variables
    globalAddr = 0x82;
    globalSize = allocateVarStorage(symbolTable);

    // now process all function local variables
    initStackFrames();
    calcLocalVarAllocs();
    allocateStackFrameStorage();
    allocateLocalVars();

    if (compilerOptions.showVarAllocations)
        showVariableAllocations();
}