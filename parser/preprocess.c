//
//  Preprocessor for C language
//
// Created by admin on 3/29/2020.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <machine/machine.h>

#include "common/common.h"
#include "parse_directives.h"
#include "preprocess.h"

static char *curProg;   // current program


PreProcessInfo * initPreprocessor() {
    PreProcessInfo *preProcessInfo = allocMem(sizeof(PreProcessInfo));
    preProcessInfo->numFiles = 0;
    preProcessInfo->machine = 0;
    return preProcessInfo;
}

void addIncludeFile(PreProcessInfo *preProcessInfo, char *fileName) {
    if (preProcessInfo->numFiles < 12) {
        preProcessInfo->includedFiles[preProcessInfo->numFiles++] = fileName;
        printf("Including: %s\n", fileName);
    } else {
        printf("Warning: Too many included files\n");
    }
}

/**
 *  Read another line from the current program
 */
char* PP_readLine(const char *buffer) {
    char *curLine;
    int col = 0;
    while ((curProg[col] != '\n') && (curProg[col] != '\0') && (col < 98)) col++;

    // reset curline pointer back to beginning of line buffer
    curLine = (char *)buffer;

    strncpy(curLine, curProg, col);
    curLine[col] = '\n';
    curLine[col+1] = '\0';
    return curLine;
}

void PP_nextLine() {
    while (*curProg != '\n' && *curProg != '\r' && *curProg != '\0') curProg++;
    while (*curProg == '\n' || *curProg == '\r') curProg++;  // skip newline chars
}

//---------------------------------------------------------


void PP_Include(PreProcessInfo *preProcessInfo) {
    if (preProcessInfo->numFiles < 12) {
        char *includeFile = strtok(NULL, " \n\r");
        char *fileName = getUnquotedString(includeFile);
        preProcessInfo->includedFiles[preProcessInfo->numFiles++] = fileName;
        printf("Including: %s\n", fileName);
    } else {
        printf("Warning: Too many included files\n");
    }
}

void PP_Machine(PreProcessInfo *preProcessInfo) {
    char *machineName = getUnquotedString(strtok(NULL, "\n\r"));
    enum Machines machine = lookupMachineName(machineName);
    if (machine > 0) {
        preProcessInfo->machine = machine;
        prepForMachine(machine);
    } else {
        printf("Error!  Failed to lookup machine name: %s\n", machineName);
    }
    free(machineName);
}

void PP_Directive(char *curLine, PreProcessInfo *preProcessInfo) {
    char *directive = strtok(curLine," \n\r");

    enum CompilerDirectiveTokens directiveToken = lookupDirectiveToken(directive);
    switch (directiveToken) {
        case INCLUDE:      PP_Include(preProcessInfo); break;
        case MACHINE_DEF:  PP_Machine(preProcessInfo); break;
        case UNKNOWN_DIRECTIVE:
            printf("Warning: Unknown directive: %s\n", directive);
        default: break;
    }
}

void preprocess(PreProcessInfo *preProcessInfo, char *inFileStr) {
    printf("Pre-processing...\n");

    /* find inFileStr length */
    int inFileLen = 0;
    while (inFileStr[inFileLen] != '\0') inFileLen++;

    curProg = inFileStr;

    // allocate line buffer
    char *lineBuffer = (char *)allocMem(100);

    while (curProg < (inFileStr + inFileLen)) {
        char *curLine = PP_readLine(lineBuffer);

        // eat white space
        while (*curLine == ' ') curLine++;

        if (*curLine == '#') {
            // found a preparse statement
            curLine++;
            PP_Directive(curLine, preProcessInfo);
        }
        PP_nextLine();
    }
    free(lineBuffer);
}