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
// Created by admin on 3/29/2020.
//

#ifndef MODULE_PREPROCESS_H
#define MODULE_PREPROCESS_H

typedef struct {
    int numFiles;
    char *includedFiles[12];
    enum Machines machine;
} PreProcessInfo;

extern PreProcessInfo * initPreprocessor();
extern void addIncludeFile(PreProcessInfo *preProcessInfo, char *fileName);
extern void preprocess(PreProcessInfo *preProcessInfo, char *inFileStr);

#endif //MODULE_PREPROCESS_H
