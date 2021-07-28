//
// Created by User on 7/27/2021.
//

#include <stdio.h>
#include "write_output.h"

static FILE *outputFile;

//---------------------------------------------------------
// Output Adapter API

void WriteBIN_Init(FILE *outFile);
void WriteBIN_Done();
char* WriteBIN_getExt();
void WriteBIN_FunctionBlock(const OutputBlock *block);
void WriteBIN_StaticArrayData(const OutputBlock *block);
void WriteBIN_StaticStructData(const OutputBlock *block);

struct OutputAdapter BIN_OutputAdapter =
        {
                &WriteBIN_Init,
                &WriteBIN_Done,
                &WriteBIN_getExt,
                &WriteBIN_FunctionBlock,
                &WriteBIN_StaticArrayData,
                &WriteBIN_StaticStructData
        };

//---------------------------------------------------------

void WriteBIN_Init(FILE *outFile) {}
void WriteBIN_Done() {}
char* WriteBIN_getExt() { return ".bin"; }
void WriteBIN_FunctionBlock(const OutputBlock *block) {}
void WriteBIN_StaticArrayData(const OutputBlock *block) {}
void WriteBIN_StaticStructData(const OutputBlock *block) {}