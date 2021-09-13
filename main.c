/*
 *   Neolithic Compiler v0.2 - Simple C Cross-compiler for the 6502
 *
**/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/common.h"
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

//-------------------------------------------
//  Global Variables

CompilerOptions compilerOptions;
PreProcessInfo *preProcessInfo;
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

    char *fileData = allocMem(fsize + 1);

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


void loadAndParseAllDependencies() {
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


void generateCodeForDependencies() {
    for_range(curFileIdx, 0, preProcessInfo->numFiles) {
        char *curFileName = preProcessInfo->includedFiles[curFileIdx];
        ListNode progNode = SourceFileList_lookupAST(curFileName);
        generate_code(curFileName, progNode);
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

    preprocess(preProcessInfo, mainFileData);

    if (preProcessInfo->machine == Machine_Unknown) {
        printf("Unknown machine specified, cannot continue!\n");
        return -1;
    }

    bool hasDependencies = (preProcessInfo->numFiles > 0);
    if (hasDependencies) {
        // Build and output AST, then process and analyze symbols
        loadAndParseAllDependencies();
    }

    reportMem();

    if (GC_ErrorCount > 0) return -1;

    // now build AST and symbols for main file
    ListNode mainProgNode = parse(inFileName, mainFileData);
    writeParseTree(mainProgNode, inFileName);

    if (parserErrorCount != 0) return -1;

    //--------------------------------------------------------
    //--- Now compile!

    reportMem();

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
    initCodeGenerator(mainSymbolTable);

    //   Now do full compile on any dependencies
    if (hasDependencies) {
        generateCodeForDependencies();
    }

    //------------------------------------------
    //----- Compile main file

    printf("Compiling main program %s\n", inFileName);
    ListNode progNode = SourceFileList_lookupAST(inFileName);
    generate_code(inFileName, progNode);

    if (GC_ErrorCount == 0) {
        //OPT_RunOptimizer();

        if (compilerOptions.showOutputBlockList) {
            printf("Output layout:\n");
            OB_PrintBlockList();
        }

        //------------------------------------------
        // Output ASM code and BIN file

        WriteOutput(projectName, OUT_DASM, mainSymbolTable);
        WriteOutput(projectName, OUT_BIN, mainSymbolTable);
    } else {
        printf("Unable to process program due to errors\n");
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


//------------------------------------------------------------------------------------------

bool showMemoryUsage;
enum Machines targetMachine;

void reportMemoryUsage() {
    printf("\n\nsizeof SymbolRecord = %d\n", sizeof(SymbolRecord));
    printf("sizeof SymbolExtStruct = %d\n", sizeof(struct SymbolExtStruct));
    printf("sizeof LabelStruct = %d\n", sizeof(struct LabelStruct));
    printf("sizeof ListStruct = %d\n", sizeof(struct ListStruct));
    printf("sizeof ListNode = %d\n", sizeof(ListNode));

    printParseTreeMemUsage();

    printInstrListMemUsage();

    //printHashTable();

    printf("Other ");
    reportMem();
}

void setDefaultCompilerParameters() {
    targetMachine = Atari2600;

    showMemoryUsage = false;
    compilerOptions.entryPointFuncName = "main";

    compilerOptions.showCallTree = false;
    compilerOptions.maxFuncCallDepth = 3;

    compilerOptions.showVarAllocations = false;

    compilerOptions.reportFunctionProcessing = false;

    compilerOptions.showOutputBlockList = false;
}

/**
 * Parse command line parameters
 *
 * Potential options:
 *   -a (analyze) (assembly)
 *   -b (build) (binary)
 *   -c (config)
 *   -d (debug)
 *   -e (entry-point) (error)
 *   -f (function map)
 *   -g (generate)  (a=assembly, b=binary)
 *   -h (HELP)
 *   -i (info) (include)
 *   -j
 *   -k
 *   -l (layout)
 *   -m (machine) (module) (memory)
 *   -n
 *   -o (optimize) (output)
 *   -p (project)
 *   -q (quiet)
 *   -r (report)
 *   -s (show) (set)
 *   -t (target)
 *   -u
 *   -v (view) (variables) (verbose)
 *   -va (view allocations)
 *   -vc (view call tree)
 *   -vl (view layout of memory)
 *   -w (warnings)
 *   -x
 *   -y
 *   -z
 *
 * @param argc
 * @param argv
 */

void parseCommandLineParameters(int argc, char *argv[]) {
    // argv[0] = full path to executable? (path + name)  TODO: might just be portion of command line used to call exe
    // argv[1] = (infile)
    // argv[2++] = params

    // Process any command line parameters
    for (int c=2; c<argc; c++) {
        char *cmdParam = argv[c];

        if ((cmdParam[0] == '-') && (cmdParam[1] != 0)) switch(cmdParam[1]) {
            case 'e':
                printf("Change entry point name to: %s\n", (cmdParam + 2));
                break;
            case 'f':
                printf("Show function map: ON\n");
                compilerOptions.showCallTree = true;
                break;
            case 'i':
                addIncludeFile(preProcessInfo, newSubstring(cmdParam, 2, 2));
                break;
            case 'l':
                printf("Show output block layout: ON\n");
                compilerOptions.showOutputBlockList = true;
                break;
            case 'm':
                showMemoryUsage = true;
                printf("Show memory usage: ON\n");
                break;
            case 'v':
                if (cmdParam[2] != 0) switch (cmdParam[2]) {
                    case 'a': compilerOptions.showVarAllocations = true; break;
                    case 'c': compilerOptions.showCallTree = true; break;
                    case 'l': compilerOptions.showOutputBlockList = true; break;
                    default:
                        printf("Unknown view option\n");
                        break;
                } else {
                    printf("View parameter not specified\n");
                }
                break;
            default:
                printf("Arg %d: %s\n", c, cmdParam);
        }
    }
}

//------------------------------------------------------

int main(int argc, char *argv[]) {
    printf("Neolithic Compiler v%s - Simplified C Cross-compiler for the 6502\n", verStr);

    if (argc < 2) {
        printf("Usage:\tneolithic (infile)\n");
        return -1;
    }

    // Initialize the preprocessor so that the command line parameter parser has access to it.
    preProcessInfo = initPreprocessor();

    // TODO: split directory from file name, if any
    projectDir = "";
    projectName = argv[1];

    inFileName = genFileName(projectName, ".c");

    // Process Compiler Parameters
    setDefaultCompilerParameters();
    if (argc > 2) parseCommandLineParameters(argc, argv);

    prepForMachine(targetMachine);

    int result = mainCompiler();        // Call the main compiler

    if (showMemoryUsage) reportMemoryUsage();

    return result;
}