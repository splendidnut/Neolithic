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
// Created by admin on 6/14/2020.
//

#ifndef MODULE_COMMON_H
#define MODULE_COMMON_H

#include <stdbool.h>

// Fun macro for doing a simple 'for' loop
// TODO: Maybe add to Neolithic language spec?

#define for_range(varname, startvalue, endvalue) \
    for (int varname = (startvalue); varname < (endvalue); varname++)

#define freeIfNotNull(data) \
    if ((data) != NULL) free(data);

/**
 * Simple structure to encapsulate information to retrieve lines from the source code file
 * for use in commenting the assembly output.
 */
typedef struct {
    int lineNum;
    int len;
    char *data;
} SourceCodeLine;

typedef struct {
    char *entryPointFuncName;
    bool showGeneralInfo;
    bool showCallTree;
    bool showVarAllocations;
    bool showOutputBlockList;
    bool reportFunctionProcessing;
    char maxFuncCallDepth;
    bool runOptimizer;
} CompilerOptions;

extern CompilerOptions compilerOptions;

extern void *allocMem(unsigned int size);
extern void freeMem(void *mem);
extern void reportMem();

extern char *numToStr(int num);
extern char *intToStr(int num);
extern int strToInt(const char *str);
extern char *getStructRefComment(const char *prefixComment, const char *structName, const char *propName);
extern char *newSubstring(const char* inStr, int idxBegin, int idxEnd);
extern char *getUnquotedString(const char *srcString);
extern char *genFileName(const char *name, const char *ext);
extern char *buildSourceCodeLine(const SourceCodeLine *srcStr);

#endif //MODULE_COMMON_H
