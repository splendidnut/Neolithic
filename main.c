/***************************************************************************
 * Neolithic Compiler - Simple C Cross-compiler for the 6502
 *
 * Copyright (c) 2020-2022 by Philip Blackman
 * -------------------------------------------------------------------------
 *
 * Licensed under the GNU General Public License v2.0
 *
 * See the "LICENSE.TXT" file for more information regarding usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * -------------------------------------------------------------------------
 */

/*
 *   Neolithic Compiler v0.4 - Simple C Cross-compiler for the 6502
 *
 *   New in alpha 0.4
 *   -> Working on get other Machine support running (like Atari7800)
 *   -> Working on banking / bankswitching support
 *
 *
 *   TODO:  Fix compile steps to allow included files to be preprocessed.
 *              This requires separating out loadAndParse into separate pieces,
 *              so that the files can all be loaded and preprocessed (allow recursion
 *              into deeper includes).  Then after all that is done, they can be parsed.
 *
 *              Finally, everything can be compiled.
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
#include "output/output_manager.h"
#include "output/write_output.h"
#include "optimizer/optimizer.h"

const char *verStr = "0.4(beta)";

//-------------------------------------------
//  Global Variables

CompilerOptions compilerOptions;
PreProcessInfo *preProcessInfo;
SymbolTable * mainSymbolTable;
char *projectName;
char *projectDir;
char *inFileName;
bool showMemoryUsage;
enum Machines targetMachine;

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

    if (compilerOptions.showGeneralInfo) printf("Loading file %-20s  ", fileToLoad);
    FILE *inFile = fopen(fileToLoad, "r");

    if (!inFile) {
        printf("Unable to open %s\n", fileName);
        return NULL;
    }

    fseek(inFile, 0, SEEK_END);
    long fsize = ftell(inFile);
    fseek(inFile, 0, SEEK_SET);  /* same as rewind(f); */

    if (compilerOptions.showGeneralInfo) printf("%ld bytes\n", fsize);

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
        progNode = parse_program(sourceCode, curFileName);
        SourceFileList_add(curFileName, sourceCode, progNode);
    } else {
        printf("File has already been loaded and parsed: %s\n", curFileName);
        return createEmptyNode();
    }
    return progNode;
}


void loadAndParseAllDependencies() {
    char *srcFileName, *srcFileData;
    for_range(curFileNum, 0, preProcessInfo->numFiles) {
        srcFileName = preProcessInfo->includedFiles[curFileNum];
        srcFileData = readSourceFile(srcFileName);
        if (srcFileData == NULL) {
            printf("ERROR: Missing dependency %s\n", srcFileName);
            exit(-1);
        }
        ListNode progNode = parse(srcFileName, srcFileData);
        if (progNode.type != N_EMPTY) {
            writeParseTree(progNode, srcFileName);

            if (parserErrorCount != 0) break;

            generate_symbols(progNode, mainSymbolTable);
        }
    }
}

void generateCallTreeForDependencies() {
    for_range(curFileIdx, 0, preProcessInfo->numFiles) {
        char *curFileName = preProcessInfo->includedFiles[curFileIdx];
        ListNode progNode = SourceFileList_lookupAST(curFileName);
        generate_callTree(progNode, mainSymbolTable, false);
    }
}

void generateCodeForDependencies() {
    for_range(curFileIdx, 0, preProcessInfo->numFiles) {
        char *curFileName = preProcessInfo->includedFiles[curFileIdx];
        ListNode progNode = SourceFileList_lookupAST(curFileName);
        generate_code(curFileName, progNode);
    }
}

void check_for_entry_point() {
    if (!findSymbol(mainSymbolTable, "main")) {
        char *entryPointName = "main()";
        printf("Warning:  Missing entry point function:  %s not found.\n", entryPointName);
        GC_ErrorCount++;
    }
}

int mainCompiler() {
    char* mainFileData = readSourceFile(inFileName);
    if (mainFileData == NULL) {
        printf("Unable to open file\n");
        return -1;
    }

    SourceFileList_init();
    mainSymbolTable = initSymbolTable("main", NULL);
    preprocess(preProcessInfo, mainFileData);

    //---------------------------------------------------
    // check to make sure we have a machine configured

    if (preProcessInfo->machine > Machine_Unknown) {
        targetMachine = preProcessInfo->machine;
    } else if (targetMachine == Machine_Unknown) {
        printf("Unknown machine specified, cannot continue!\n");
        return -1;
    }

    //------------------------------------------------------
    // Start processing dependencies

    bool hasDependencies = (preProcessInfo->numFiles > 0);
    if (hasDependencies) {
        // Build and output AST, then process and analyze symbols
        loadAndParseAllDependencies();
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
    if (compilerOptions.showGeneralInfo) printf("Symbol Table generation Complete\n");

    if (hasDependencies) {
        generateCallTreeForDependencies();
    }
    generate_callTree(mainProgNode, mainSymbolTable, true);
    generate_var_allocations(mainSymbolTable);

    if (compilerOptions.showGeneralInfo) printf("Analysis of %s Complete\n", inFileName);

    //-----------------------------------------------------------

    initOutputGenerator(targetMachine);
    initCodeGenerator(mainSymbolTable, targetMachine);

    //   Now do full compile on any dependencies
    if (hasDependencies) {
        generateCodeForDependencies();
    }

    //------------------------------------------
    //----- Compile main file

    if (compilerOptions.showGeneralInfo) printf("Compiling main program %s\n", inFileName);
    ListNode progNode = SourceFileList_lookupAST(inFileName);
    generate_code(inFileName, progNode);

    check_for_entry_point();

    //----- IF Compiled successfully, THEN do post-processing and output.
    if (GC_ErrorCount == 0) {
        //OB_BuildInitialLayout();
        //OB_ArrangeBlocks();

        if (compilerOptions.showOutputBlockList) {
            printf("\nOutput layout:\n");
            OB_PrintBlockList();
        }

        //------------------------------------------
        // Output ASM code and BIN file

        OutputFlags outputFlags;
        outputFlags.doOutputASM = true;
        outputFlags.doOutputBIN = true;
        generateOutput(projectName, mainSymbolTable, outputFlags);
    } else {
        printf("Unable to process program due to (%d) errors\n", GC_ErrorCount);
    }

    //--- Finish off by outputting the symbol table
    writeSymbolTable(inFileName);

    //----- Cleanup!
    if (compilerOptions.showGeneralInfo) printf("Cleaning up\n");

    free(preProcessInfo);
    free(mainFileData);

    SourceFileList_cleanup();
    killSymbolTable(mainSymbolTable);

    return 0;
}


//------------------------------------------------------------------------------------------


void reportMemoryUsage() {
    printf("\n\nsizeof SymbolRecord = %d\n", (int)sizeof(SymbolRecord));
    printf("sizeof LabelStruct = %d\n", (int)sizeof(struct LabelStruct));
    printf("sizeof ListStruct = %d\n", (int)sizeof(struct ListStruct));
    printf("sizeof ListNode = %d\n", (int)sizeof(ListNode));

    printParseTreeMemUsage();

    printInstrListMemUsage();

    //printHashTable();

    printf("Other ");
    reportMem();
}

void setDefaultCompilerParameters() {
    targetMachine = Atari2600;

    compilerOptions.showGeneralInfo = true;

    showMemoryUsage = false;
    compilerOptions.entryPointFuncName = "main";

    compilerOptions.showCallTree = false;
    compilerOptions.maxFuncCallDepth = 3;

    compilerOptions.showVarAllocations = false;

    compilerOptions.reportFunctionProcessing = false;

    compilerOptions.showOutputBlockList = false;

    compilerOptions.runOptimizer = false;
    compilerOptions.showOptimizerSteps = false;
}

/**
 * Parse command line parameters
 *
 * Potential options:
 *   -a (analyze) (assembly)
 *   -b (build) (binary)
 *   -c (config)
 *   -f (function map)
 *   -g (generate)  (a=assembly, b=binary)
 *   -j
 *   -k
 *   -l (layout)
 *   -n
 *   -o (optimize) (output)
 *   -p (project)
 *   -q (quiet)
 *   -r (report)
 *   -s (show) (set)
 *   -t (target)
 *   -u
 *   -w (warnings)
 *   -x
 *   -y
 *   -z
 *
 * @param argc
 * @param argv
 */

const char *help[] = {
        "Command Line Options",
        "  -d  Show debugging information for the compiler itself ",
        "         (Memory usage)",
        "  -e  Change name of Entry point",
        "  -f  Show function call tree",
        "  -h  Show help for Command Line options",
        "  -i  Include file",
        "  -m  Select machine target",
        "  -o  Run Optimizer on generated machine code",
        "        -o   Run optimizer without logging",
        "        -ov  Show log of optimizations",
        "  -v  View details about:",
        "        -va  Show variable allocations",
        "        -vc  Show call tree",
        "        -vl  Show output block layout"
};

void showCmdParamHelp() {
    printf("\n");
    for_range(lineNum, 0, sizeof(help) / sizeof(char *)) {
        printf("%s\n", help[lineNum]);
    }
    printf("\n");
}

void parseCommandLineParameters(int argc, char *argv[]) {
    // argv[0] = full path to executable? (path + name)  TODO: might just be portion of command line used to call exe
    // argv[1] = (infile)
    // argv[2++] = params

    // Process any command line parameters
    for (int c=2; c<argc; c++) {
        char *cmdParam = argv[c];

        if ((cmdParam[0] == '-') && (cmdParam[1] != 0)) switch(cmdParam[1]) {
            // Debug options for the compiler itself
            case 'd':
                showMemoryUsage = true;
                printf("Show memory usage: ON\n");
                break;

            case 'e':
                printf("Change entry point name to: %s\n", (cmdParam + 2));
                break;

            case 'f':
                printf("Show function map: ON\n");
                compilerOptions.showCallTree = true;
                break;

            case 'h':
                showCmdParamHelp();
                break;

            case 'i':
                addIncludeFile(preProcessInfo, newSubstring(cmdParam, 2, 2));
                break;

            case 'm': {
                char *machineName = newSubstring(cmdParam, 2, 2);
                printf("Machine lookup: %s\n", machineName);
                enum Machines machine = lookupMachineName(machineName);
                if (machine > 0) {
                    printf("Found!\n");
                    targetMachine = machine;
                }
            } break;

            case 'o':
                if (cmdParam[2] == 'v') {
                    compilerOptions.showOptimizerSteps = true;
                }
                compilerOptions.runOptimizer = true;
                break;

            case 'q':
                compilerOptions.showGeneralInfo = false;
                compilerOptions.showOutputBlockList = false;
                compilerOptions.showCallTree = false;
                compilerOptions.showVarAllocations = false;
                compilerOptions.reportFunctionProcessing = false;
                break;

            case 'v':
                if (cmdParam[2] != 0) switch (cmdParam[2]) {
                    case 'a': compilerOptions.showVarAllocations = true; break;
                    case 'c': compilerOptions.showCallTree = true; break;
                    case 'r': compilerOptions.reportFunctionProcessing = true; break;
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
    printf("\nNeolithic Compiler v%s (%s) - Simplified C Cross-compiler for the 6502\n\n", verStr, __DATE__);

    if (argc < 2) {
        printf("Usage:\tneolithic (infile) (options)\n");
        showCmdParamHelp();
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