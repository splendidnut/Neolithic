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
