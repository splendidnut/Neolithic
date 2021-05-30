//
// Created by admin on 6/14/2020.
//

#ifndef MODULE_COMMON_H
#define MODULE_COMMON_H

typedef struct {
    int lineNum;
    int len;
    char *data;
} SourceCodeLine;

extern char *numToStr(int num);
extern char *intToStr(int num);
extern char *buildSourceCodeLine(const SourceCodeLine *srcStr);

#endif //MODULE_COMMON_H
