//
// Created by admin on 6/14/2020.
//

#ifndef MODULE_COMMON_H
#define MODULE_COMMON_H

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
} CompilerOptions;

extern CompilerOptions *compilerOptions;

extern char *numToStr(int num);
extern char *intToStr(int num);
extern char *buildSourceCodeLine(const SourceCodeLine *srcStr);

#endif //MODULE_COMMON_H
