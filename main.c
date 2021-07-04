/*
 *   Neolithic Compiler v0.1 - Simple C Cross-compiler for the 6502
 *
**/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data/syntax_tree.h"
#include "machine/machine.h"
#include "machine/mem.h"
#include "parser/parser.h"
#include "parser/preprocess.h"
#include "codegen/gen_symbols.h"
#include "codegen/gen_alloc.h"
#include "codegen/gen_calltree.h"
#include "codegen/gen_code.h"

const char *verStr = "0.1(alpha)";

#define DEBUG

//-------------------------------------------
//  Global Variables

CompilerOptions *compilerOptions;
SymbolTable * mainSymbolTable;
MemoryArea *memForAtari2600;
char *projectName;
char *projectDir;
char *inFileName;
char *outFileName;

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

void writeParseTree(ListNode treeRoot, char *name) {
    // if no name, show nothing
    if (name == NULL) return;

    // write out the abstract syntax tree
    char *astFileName = genFileName(name, ".ast");
    FILE *astFile = fopen(astFileName, "w");
    showParseTree(treeRoot, astFile);
    fclose(astFile);

    free(astFileName);
}


void writeSymbolTable(const char *name) {
    if (name == NULL) return;

    char *symFileName = genFileName(name, ".sym");
    FILE *symFile = fopen(symFileName, "w");
    showSymbolTable(symFile, mainSymbolTable);
    fclose(symFile);

    free(symFileName);
}

//----------------------------------------------------------------------------------------

ListNode parse(char *curFileName, char *sourceCode, bool doWriteAst) {
    ListNode progNode = parse_program(sourceCode);

    // now print the parse tree
    if (doWriteAst) {
        writeParseTree(progNode, curFileName);
    }
    return progNode;
}

void compilePass1(char *inputSrc, char *curFileName, bool doWriteAst, bool isTimeToAlloc) {
    ListNode progNode = parse(curFileName, inputSrc, doWriteAst);
    if (parserErrorCount == 0) {
        generate_symbols(progNode, mainSymbolTable);
        if (isTimeToAlloc) {
            generate_callTree(progNode, mainSymbolTable);
            generate_var_allocations(mainSymbolTable, memForAtari2600);
        }
        printf("Analysis of %s Complete\n\n", curFileName);
    }
}

void compilePass2(char *inputSrc, char *srcFileName, FILE *codeOutFile, bool doWriteSym, bool doWriteAsm) {
    ListNode progNode = parse(srcFileName, inputSrc, false);
    if (parserErrorCount == 0) {

        //generate_callTree(progNode);

        generate_code(progNode, mainSymbolTable, codeOutFile, doWriteAsm);

        // now show symbol list
        if (doWriteSym) {
            writeSymbolTable(srcFileName);
        }
    }
}


/*---------------------------
//   Main method
*/


void processDependencies(PreProcessInfo *preProcessInfo, int pass) {
    int cntDependencies = preProcessInfo->numFiles;
    int curFileNum = 0;

    printf("\nProcessing dependencies: %d\n", cntDependencies);
    while (curFileNum < cntDependencies) {
        char *curFileName = preProcessInfo->includedFiles[curFileNum];
        char *subFileData = readSourceFile(curFileName);
        if  (subFileData != NULL) {
            printf("Compiling pass %d: %s\n", pass, curFileName);

            FILE *codeOutfile = stdout;
            switch (pass) {
                case 1:
                    compilePass1(subFileData, curFileName, true, false);
                    break;
                case 2:
                    compilePass2(subFileData, curFileName, codeOutfile, false, false);
                    break;
            }
            free(subFileData);
        } else {
            printf("ERROR: Missing dependency %s\n", curFileName);
            exit(-1);
        }
        curFileNum++;
    }
}

int mainCompiler() {
    printf("\n");
    printf("Initializing symbol table\n");
    mainSymbolTable = initSymbolTable("main", true);

    char* mainFileData;
    if ((mainFileData = readSourceFile(inFileName))) {
        PreProcessInfo *preProcessInfo = preprocess(mainFileData);

        if (preProcessInfo->numFiles > 0) {
            processDependencies(preProcessInfo, 1); // Build AST and analyize symbols
            compilePass1(mainFileData, inFileName, true, true);
            processDependencies(preProcessInfo, 2); //   Now do full subcompile
        } else {
            // no dependencies, so just build AST and symbols for main file
            compilePass1(mainFileData, inFileName, true, true);
        }

        //----- Compile main file
        printf("Compiling %s to %s\n", inFileName, outFileName);

        FILE *outfile = fopen(outFileName, "w");
        compilePass2(mainFileData, inFileName, outfile, true, true);
        fclose(outfile);

        //----- Cleanup!
        free(preProcessInfo);
        free(mainFileData);
    } else {
        printf("Unable to open file\n");
    }
    printf("Cleaning up\n");
    killSymbolTable(mainSymbolTable);
}

//===========================================================================
//  deal with machine specific stuff

void prepForMachine(enum Machines machine) {
    MemoryArea *zeropageMem;

    // TODO: remove old method
    memForAtari2600 = createMemoryRange(0x82, 0xFF);

    switch (machine) {
        case Atari2600:
            SMA_init(0);        // only has zeropage memory
            zeropageMem = SMA_addMemoryRange(0x80, 0xFF);

            break;
        case Atari7800:
            // atari7800 memory ranges:
            //    zeropage = 0x40..0xFF
            //      stack  = 0x140..0x1FF
            //    mem1     = 0x1800..0x1FFF     -- 2k
            //    mem2     = 0x2200..0x27FF     -- 1.5k

            SMA_init(1);        // save zeropage memory for other things
            zeropageMem = SMA_addMemoryRange(0x40, 0xFF);
            SMA_addMemoryRange(0x1800, 0x1FFF);
            SMA_addMemoryRange(0x2200, 0x27FF);
            break;
        default:
            printf("Unknown machine!\n");
    }
    // Allocate two byte for 16-bit accumulator in zeropage space
    SMA_allocateMemory(zeropageMem, 2);
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
    projectDir = "c:\\atari2600\\projects\\congo_c\\";
    projectName = "congo";
#endif

    inFileName = genFileName(projectName, ".c");
    outFileName = genFileName(projectName, ".asm");

    compilerOptions = malloc(sizeof(CompilerOptions));
    compilerOptions->entryPointFuncName = "main";

    prepForMachine(Atari2600);
    mainCompiler();

    free(compilerOptions);
}