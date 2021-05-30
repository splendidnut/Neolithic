//
//  Preprocessor for C language
//
// Created by admin on 3/29/2020.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preprocess.h"

static char *curProg;   // current program

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

        // all this just to trim quotes off
        unsigned int fileNameLen = strlen(includeFile) - 2;
        char *fileName = malloc(fileNameLen+1);
        strncpy(fileName, includeFile+1, fileNameLen);
        fileName[fileNameLen] = '\0';

        preProcessInfo->includedFiles[preProcessInfo->numFiles++] = fileName;
        printf("Including: %s\n", fileName);
    } else {
        printf("Warning: Too many included files\n");
    }
}

void PP_Machine(PreProcessInfo *preProcessInfo) {
    //preProcessInfo;
}

void PP_Directive(char *curLine, PreProcessInfo *preProcessInfo) {
    char *directive = strtok(curLine," \n\r");

    if (strncmp(directive, "include", 7) == 0) {
        PP_Include(preProcessInfo);
    } else if (strncmp(directive, "machine", 7) == 0) {
        PP_Machine(preProcessInfo);
    } else {
        printf("Warning: Unknown directive: %s\n", directive);
    }
}


PreProcessInfo * preprocess(char *inFileStr) {
    PreProcessInfo *preProcessInfo = malloc(sizeof(PreProcessInfo));
    preProcessInfo->numFiles = 0;

    printf("Pre-processing...\n");

    /* find inFileStr length */
    int inFileLen = 0;
    while (inFileStr[inFileLen] != '\0') inFileLen++;

    curProg = inFileStr;

    // allocate line buffer
    char *lineBuffer = (char *)malloc(100);

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

    return preProcessInfo;
}