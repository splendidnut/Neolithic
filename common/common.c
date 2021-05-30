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
