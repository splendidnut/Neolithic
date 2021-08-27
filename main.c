/*
 *   Neolithic Compiler v0.2 - Simple C Cross-compiler for the 6502
 *
**/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data/syntax_tree.h"
#include "machine/machine.h"
#include "parser/parser.h"
#include "parser/preprocess.h"
#include "codegen/gen_common.h"
#include "codegen/gen_symbols.h"
#include "codegen/gen_alloc.h"
#include "codegen/gen_calltree.h"
#include "codegen/gen_code.h"
#include "cpu_arch/instrs.h"
#include "output/write_output.h"

const char *verStr = "0.2(alpha)";

//#define DEBUG

//-------------------------------------------
//  Global Variables

CompilerOptions *compilerOptions;
SymbolTable * mainSymbolTable;
char *projectName;
char *projectDir;
char *inFileName;

//------------------------------------------
//  Cache all the source files

typedef struct {
    char *name;
    char *sourceCode;
    ListNode ast;
} SourceFile;

SourceFile parsedFiles[20];
int numSrcFiles;

void SourceFileList_init() {
    memset(parsedFiles, 0, sizeof parsedFiles);
    numSrcFiles = 0;
}

ListNode SourceFileList_lookupAST(char *astFileName) {
    for_range(curAST, 0, numSrcFiles) {
        if (strcmp(astFileName, parsedFiles[curAST].name) == 0) {
            return parsedFiles[curAST].ast;
        }
    }
    return createEmptyNode();
}

void SourceFileList_add(char *name, char *srcCode, ListNode ast) {
    parsedFiles[numSrcFiles].name = name;
    parsedFiles[numSrcFiles].sourceCode = srcCode;
    parsedFiles[numSrcFiles].ast = ast;
    numSrcFiles++;
}

void SourceFileList_cleanup() {
    for_range(idx, 0, numSrcFiles) {
        freeIfNotNull(parsedFiles[numSrcFiles].sourceCode);
    }
}

//----------------------------------------------------------------------

char* readSourceFile(const char* fileName) {
    char fileToLoad[100];
    sprintf(fileToLoad, "%s%s", projectDir, fileName);

    printf("Loading file %-20s  ", fileToLoad);
    FILE *inFile = fopen(fileToLoad, "r");

    if (!inFile) {
        printf("Unable to open\n");
        return NULL;
    }

    fseek(inFile, 0, SEEK_END);
    long fsize = ftell(inFile);
    fseek(inFile, 0, SEEK_SET);  /* same as rewind(f); */

    printf("%ld bytes\n", fsize);

    char *fileData = malloc(fsize + 1);

    // Since fread() will magically convert end of line characters
    //  in a text file (windows CRLF -> LF), we need to use
    //  the size returned to properly mark the end of the buffer.
    fsize = fread(fileData, 1, fsize, inFile);
    fclose(inFile);
    fileData[fsize] = '\0';

    return fileData;
}

//---------------------------------------------------------------------

void showParseTree(ListNode node, FILE *outputFile) {
    if (node.type == N_LIST) {
        List *list = node.value.list;

        fprintf(outputFile, "Abstract syntax tree:\n");
        showList(outputFile, list, 0);
        fprintf(outputFile, "\n");
    } else {
        printf("Error: No parse tree.  Failed to parse\n");
    }
}

void writeParseTree(ListNode treeRoot, char *name) {
    // if no name, show nothing
    if (name == NULL) return;

    // write out the abstract syntax tree
    char *astFileName = genFileName(name, ".ast");
    FILE *astFile = fopen(astFileName, "w");
    if (astFile) {
        showParseTree(treeRoot, astFile);
        fclose(astFile);
    }
    freeIfNotNull(astFileName);
}


void writeSymbolTable(const char *name) {
    if (name == NULL) return;

    char *symFileName = genFileName(name, ".sym");
    FILE *symFile = fopen(symFileName, "w");
    if (symFile) {
        showSymbolTable(symFile, mainSymbolTable);
        fclose(symFile);
    }

    freeIfNotNull(symFileName);
}

//----------------------------------------------------------------------------------------

ListNode parse(char *curFileName, char *sourceCode) {
    ListNode progNode = SourceFileList_lookupAST(curFileName);
    if (progNode.type == N_EMPTY) {
        progNode = parse_program(sourceCode);
        SourceFileList_add(curFileName, sourceCode, progNode);
    }
    return progNode;
}


void loadAndParseAllDependencies(PreProcessInfo *preProcessInfo) {
    char *srcFileName, *srcFileData;
    for_range(curFileNum, 0, preProcessInfo->numFiles) {
        srcFileName = preProcessInfo->includedFiles[curFileNum];
        printf("Loading and parsing %s\n", srcFileName);

        srcFileData = readSourceFile(srcFileName);
        if (srcFileData == NULL) {
            printf("ERROR: Missing dependency %s\n", srcFileName);
            exit(-1);
        }
        ListNode progNode = parse(srcFileName, srcFileData);

        writeParseTree(progNode, srcFileName);

        if (parserErrorCount != 0) break;

        generate_symbols(progNode, mainSymbolTable);
    }
}


void generateCodeForDependencies(PreProcessInfo *preProcessInfo) {
    for_range(curFileIdx, 0, preProcessInfo->numFiles) {
        char *curFileName = preProcessInfo->includedFiles[curFileIdx];
        ListNode progNode = SourceFileList_lookupAST(curFileName);
        generate_code(curFileName, progNode, mainSymbolTable);
    }
}

int mainCompiler() {
    char* mainFileData = readSourceFile(inFileName);
    if (mainFileData == NULL) {
        printf("Unable to open file\n");
        return -1;
    }

    SourceFileList_init();

    printf("\n");
    printf("Initializing symbol table\n");
    mainSymbolTable = initSymbolTable("main", NULL);

    PreProcessInfo *preProcessInfo = preprocess(mainFileData);
    if (preProcessInfo->machine == Machine_Unknown) {
        printf("Unknown machine specified, cannot continue!\n");
        return -1;
    }

    bool hasDependencies = (preProcessInfo->numFiles > 0);
    if (hasDependencies) {
        // Build and output AST, then process and analyze symbols
        loadAndParseAllDependencies(preProcessInfo);
    }

    if (GC_ErrorCount > 0) return -1;

    // now build AST and symbols for main file
    ListNode mainProgNode = parse(inFileName, mainFileData);
    writeParseTree(mainProgNode, inFileName);

    if (parserErrorCount != 0) return -1;

    //--------------------------------------------------------
    //--- Now compile!

    generate_symbols(mainProgNode, mainSymbolTable);
    if (GC_ErrorCount > 0) return -1;               // abort if any issues when processing symbols

    generate_callTree(mainProgNode, mainSymbolTable);
    generate_var_allocations(mainSymbolTable);

    printf("Analysis of %s Complete\n\n", inFileName);

    //-----------------------------------------------------------
    // Configure output for specific machine
    // TODO: This code is still specific to Atari 2600... need to figure out a way to eliminate that constraint.
    // only need to initial instruction list and output block modules once

    IL_Init(getMachineStartAddr(preProcessInfo->machine));
    OB_Init();


    //   Now do full compile on any dependencies
    if (hasDependencies) {
        generateCodeForDependencies(preProcessInfo);
    }

    //----- Compile main file
    ListNode progNode = SourceFileList_lookupAST(inFileName);

    //------------------------------------------
    // Output ASM code and SYM file

    printf("Compiling main program %s\n", inFileName);

    WO_Init(projectName, OUT_DASM, mainSymbolTable);

    // this needs to be done before the program is processed
    //  because the Code Generator makes changes to the symbol table
    //    (adds static data structures)

    WO_PrintSymbolTable(mainSymbolTable, "Main");

    generate_code(inFileName, progNode, mainSymbolTable);

    // need to add error check here
    if (GC_ErrorCount == 0) {
        //OPT_RunOptimizer();
        WO_WriteAllBlocks();    // output all the code and the variables

        printf("Output layout:\n");
        OB_PrintBlockList();
    } else {
        printf("Unable to process program due to errors\n");
    }

    WO_Done();

    //----------------------------------------
    //  Output binary

    if (GC_ErrorCount == 0) {
        WO_Init(projectName, OUT_BIN, mainSymbolTable);
        WO_WriteAllBlocks();
        WO_Done();
    }

    //--- Finish off by outputting the symbol table
    writeSymbolTable(inFileName);

    //----- Cleanup!
    free(preProcessInfo);
    free(mainFileData);

    printf("Cleaning up\n");
    SourceFileList_cleanup();
    killSymbolTable(mainSymbolTable);

    return 0;
}



int main(int argc, char *argv[]) {
    printf("Neolithic Compiler v%s - Simplified C Cross-compiler for the 6502\n", verStr);

#ifndef DEBUG
    if (argc < 2) {
        printf("Usage:\tneolithic (infile)\n");
        return -1;
    }
    projectDir = "";
    projectName = argv[1];
#endif

#ifdef DEBUG
    projectDir = "/Users/pblackman/projects/neolithic/";
    projectName = "bca";
#endif

    inFileName = genFileName(projectName, ".c");

    compilerOptions = malloc(sizeof(CompilerOptions));
    compilerOptions->entryPointFuncName = "main";

    prepForMachine(Atari2600);
    int result = mainCompiler();

    free(compilerOptions);
    return result;
}