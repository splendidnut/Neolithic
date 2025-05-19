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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

static unsigned int memoryUsed = 0;

void *allocMem(unsigned int size) {
    memoryUsed += size;
    return malloc(size);
}

void freeMem(void *mem) {
    free(mem);
}

void reportMem() {
    printf("Memory used %d\n", memoryUsed);
}

char *numToStr(int num) {
    char *result = allocMem(10);
    if (num < 0) {
        sprintf(result, "-$%X", -num);
    } else {
        sprintf(result, "$%X", num);
    }
    return result;
}

char *intToStr(int num) {
    char *result = allocMem(10);
    sprintf(result, "%d", num);
    return result;
}

int strToInt(const char *str) {
    int resultInt;
    char *lstr = (const char *)str;

    bool isNeg = (str[0] == '-');
    if (isNeg) lstr++;

    if (lstr[1] == 'x' && lstr[0] == '0') {
        resultInt = (int)strtol(lstr + 2, NULL, 16);    // hexadecimal
    } else if (lstr[1] == 'b' && lstr[0] == '0') {
        resultInt = (int)strtol(lstr + 2, NULL, 2);    // binary
    } else if (lstr[0] == '$') {
        resultInt = (int)strtol(lstr + 1, NULL, 16);    // ASM style hex
    } else if (lstr[0] == '%') {
        resultInt = (int)strtol(lstr + 1, NULL, 2);   // ASM style binary
    } else {
        resultInt = (int)strtol(lstr, NULL, 10);
    }
    if (isNeg) resultInt = -resultInt;
    return resultInt;
}

char * getStructRefComment(const char *prefixComment, const char *structName, const char *propName) {
    char *propRefStr = allocMem(40);
    sprintf(propRefStr, "%s: %s.%s", prefixComment, structName, propName);
    return propRefStr;
}


/**
 * Copies a portion from the input string into a new, memory-allocated string)
 * @param inStr - input string
 * @param idxBegin - beginning index of portion to copy (offset into input string)
 * @param idxEnd - end index of portion to copy.  If equal to idxBegin, copy to end of string
 * @return new allocated string filled with "extracted" string
 */
char * newSubstring(const char* inStr, int idxBegin, int idxEnd) {
    int len = (int)strlen(inStr);
    int newStrLen = (idxEnd == idxBegin) ? (len - idxBegin) : (idxEnd - idxBegin);

    char *resultStr = malloc(newStrLen+1);
    memcpy(resultStr, inStr + idxBegin, newStrLen);
    resultStr[newStrLen] = '\0';
    return resultStr;
}

/**
 * Get substring located inside double quotes
 * @param srcString
 * @return string within quotes, OR empty string if quotes not found
 */
char *getUnquotedString(const char *srcString) {
    int b=0,e;

    // find start index
    const char *s = srcString;
    while ((s[b] != 0) && (s[b++] != '\"'));
    if (s[b] == 0) return "";

    // find end index
    e=b;
    do {e++;} while ((s[e] != 0) && (s[e] != '\"'));
    if (s[e] == 0) return "";

    return newSubstring(s,b,e);
}


/**
 * Generate a filename with the given extension, starting from a filename
 *   that may or may not be using the .c extension
 *
 * @param name - name with or w/o .c extension
 * @param ext - desired file extension
 * @return name with new ext
 */
char *genFileName(const char *name, const char *ext) {
    // generate file name
    unsigned int nameLen = strlen(name);

    // strip off .c extension
    if ((name[nameLen-2] == '.') && (name[nameLen-1] == 'c')) {
        nameLen -= 2;
    }

    char *astFileName = allocMem(nameLen+6);
    strcpy(astFileName, name);
    strcpy(astFileName+nameLen, ext);
    return astFileName;
}

char *catStrs(const char *str1, const char *str2) {
    char *result = allocMem(strlen(str1) + strlen(str2) + 2);
    strcpy(result, str1);
    strcat(result, str2);
    return result;
}

/**
 * Build the source code line to display in the ASM output
 *
 * This is a nice, interesting feature which works relatively well... not great, but gets the job done.
 *
 * The implementation is simple, but a bit hacky, and spread out like a spaghetti monster
 * (which is somewhat understandable):
 *
 *      - The source line is originally captured in the -Tokenizer-.
 *          Changes to the -Parser- can affect how well the source lines are captured (or, not!)
 *      - Then it's stored (hidden away) in the -Syntax Tree-.
 *      - The -Code Generator- calls this function using the data from the Syntax Tree,
 *              and inserts it into the ASM code output.
 *
 * TODO: Potentially find a better way to handle this.
 *
 * @param srcStr
 * @return
 */
char *buildSourceCodeLine(const SourceCodeLine *srcStr) {
    char *dstStr = allocMem(srcStr->len + 16);
    sprintf(dstStr, "Line #%-4d:\t", srcStr->lineNum);
    strncat(dstStr, srcStr->data, srcStr->len);
    return dstStr;
}
