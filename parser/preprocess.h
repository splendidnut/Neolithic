//
// Created by admin on 3/29/2020.
//

#ifndef MODULE_PREPROCESS_H
#define MODULE_PREPROCESS_H

typedef struct {
    int numFiles;
    char *includedFiles[12];
} PreProcessInfo;

extern PreProcessInfo * preprocess(char *inFileStr);

#endif //MODULE_PREPROCESS_H
