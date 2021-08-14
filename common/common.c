//
// Created by admin on 6/14/2020.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

char *numToStr(int num) {
    char *result = malloc(10);
    if (num < 0) {
        sprintf(result, "-$%X", -num);
    } else {
        sprintf(result, "$%X", num);
    }
    return result;
}

char *intToStr(int num) {
    char *result = malloc(10);
    sprintf(result, "%d", num);
    return result;
}

int strToInt(const char *str) {
    int resultInt;
    if (str[1] == 'x' && str[0] == '0') {
        resultInt = (int)strtol(str + 2, NULL, 16);    // hexadecimal
    } else if (str[1] == 'b' && str[0] == '0') {
        resultInt = (int)strtol(str + 2, NULL, 2);    // binary
    } else if (str[0] == '$') {
        resultInt = (int)strtol(str + 1, NULL, 16);    // ASM style hex
    } else if (str[0] == '%') {
        resultInt = (int)strtol(str + 1, NULL, 2);   // ASM style binary
    } else {
        resultInt = (int)strtol(str, NULL, 10);
    }
    return resultInt;
}

char *getUnquotedString(const char *srcString) {
    unsigned int len = strlen(srcString) - 2;
    char *newStr = malloc(len+1);
    strncpy(newStr, srcString+1, len);
    newStr[len] = '\0';
    return newStr;
}


/**
 * Generate a filename with the given extension, starting from a filename
 *   that may or may not be using the .c extension
 *
 * @param name
 * @param ext
 * @return
 */
char *genFileName(const char *name, const char *ext) {
    // generate file name
    int nameLen = strlen(name);

    // strip off .c extension
    if ((name[nameLen-2] == '.') && (name[nameLen-1] == 'c')) {
        nameLen -= 2;
    }

    char *astFileName = malloc(nameLen+6);
    strcpy(astFileName, name);
    strcpy(astFileName+nameLen, ext);
    return astFileName;
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
    char *dstStr = malloc(srcStr->len + 16);
    sprintf(dstStr, "Line #%-4d:\t", srcStr->lineNum);
    strncat(dstStr, srcStr->data, srcStr->len);
    return dstStr;
}
