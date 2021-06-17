//
// Created by admin on 4/23/2020.
//

#include <string.h>
#include <stdbool.h>

#include "cpu_arch/asm_code.h"

#include "parse_asm.h"
#include "parser.h"
#include "tokenize.h"


//-----------------------------------------------------------------

char* getIndirectAddrMode() {
    char *addrModeStr = "IND";
    char tokenChar = peekToken()->tokenStr[0];
    switch (tokenChar) {
        case ')': {   // handle basic (indirect) or (indirect),y
            accept(")");
            if (peekToken()->tokenStr[0] == ',') {
                accept(",");
                char reg = getToken()->tokenStr[0];
                if (reg == 'y' || reg == 'Y') {
                    addrModeStr = "IY";
                } else {
                    printError("Expected Y register for indirect indexed addressing mode");
                }
            }
        } break;
        case ',': {
            accept(",");
            char reg = getToken()->tokenStr[0];
            if (!(reg == 'x' || reg == 'X')) {
                printError("Expected X register for indexed indirect addressing mode");
            } else {
                addrModeStr = "IX";
            }
            accept(")");
        } break;
    }
    return addrModeStr;
}

char *getDirectAddrMode(bool forceAbs) {
    char *addrModeStr;
    addrModeStr = forceAbs ? "ABS" : "ZP";
    if (inCharset(peekToken(), ",")) {
        accept(",");
        if (inCharset(peekToken(), "XYxy")) {
            char reg = getToken()->tokenStr[0];
            if (reg == 'x' || reg == 'X') {
                addrModeStr = forceAbs ? "ABX" : "ZPX";
            } else if (reg == 'y' || reg == 'Y') {
                addrModeStr = forceAbs ? "ABY" : "ZPY";
            } else {
                printError("Expected X or Y register for indexed addressing mode");
            }
        } else {
            printError("Expected X or Y register for indexed addressing mode");
        }
    }
    return addrModeStr;
}


//-----------------------------------------------------------------
//  Parse a single assembler instruction
//
//  [mnemonic]
//  [mnemonic, addressMode, parameter]

ListNode parse_asm_instr(enum MnemonicCode instrCode) {
    List *asmInstr = createList(10);

    addNode(asmInstr, createStrNode(Mnemonics[instrCode].name));

    /*** EXIT if assembly block has ended */
    if (inCharset(peekToken(), "}")) return createListNode(asmInstr);

    /*** EXIT if we've hit next assembly instruction */
    if (lookupMnemonic(peekToken()->tokenStr) != MNE_NONE) return createListNode(asmInstr);

    /*** EXIT if asm instruction has no parameters */
    if (Mnemonics[instrCode].noParams) return createListNode(asmInstr);

    // process any mnemonic extensions
    bool forceAbs = false;
    if (peekToken()->tokenStr[0] == '.') {
        accept(".");
        char *mneExt = getToken()->tokenStr;
        switch (mneExt[0]) {
            case 'w': forceAbs = true; break;
        }
    }

    // assembly instruction has parameters, so parse them
    char *addrModeStr = "";
    ListNode paramNode;
    switch (peekToken()->tokenStr[0]) {
        case '#':                   // handle Immediate op
            accept("#");
            addrModeStr = "IMM";
            paramNode = parse_expr();
            break;
        case '(':                   // handle Indirect op
            accept("(");
            paramNode = parse_expr();
            addrModeStr = getIndirectAddrMode();
            break;
        case '<':
            accept("<");
            // TODO: this should force zeropage mode (currently unnecessary since that's the default)
        default: {
            //param = copyTokenStr(getToken());
            paramNode = parse_expr();
            addrModeStr = getDirectAddrMode(forceAbs);
        }
    }
    addNode(asmInstr, createStrNode(addrModeStr));
    addNode(asmInstr, paramNode);
    //addNode(asmInstr, createStrNode(param));

    return createListNode(asmInstr);
}


//------------------------------------------------------------
// Parse Assembly Language Block
//
// Possible Syntax results
//   - labels       = [label, labelName]
//   - equates      = [equate, name, value]
//   - instruction  = [mnemonic, ... ]

ListNode PA_create_label(char *piece) {
    List *label = createList(2);
    addNode(label, createParseToken(PT_LABEL));
    addNode(label, createStrNode(piece));
    return createListNode(label);
}

ListNode PA_create_equate(char *piece) {
    List *label = createList(3);
    addNode(label, createParseToken(PT_EQUATE));
    addNode(label, createStrNode(piece));
    addNode(label, createStrNode(copyTokenStr(getToken())));
    return createListNode(label);
}

ListNode parse_pseudo_op(char *opName) {
    if (strncmp(opName, ".byte", 5)) {
        
    }
}

ListNode parse_asmBlock() {
    List *list = createList(200);
    accept("asm");
    addNode(list, createParseToken(PT_ASM));

    // get name of asm block (causes it to create a macro)
    if (peekToken()->tokenStr[0] != '{') {
        addNode(list, createStrNode(getToken()->tokenStr));
    } else {
        addNode(list, createEmptyNode());
    }

    // read asm block
    accept("{");
    while (peekToken()->tokenStr[0] != '}') {
        char *piece = copyTokenStr(getToken());
        enum MnemonicCode mnemonicCode = lookupMnemonic(piece);
        if (piece[0] == '.') {
            addNode(list, parse_pseudo_op(piece));
        } else if (mnemonicCode != MNE_NONE) {
            addNode(list, parse_asm_instr(mnemonicCode));
        } else {
            // label or equ
            char *op = getToken()->tokenStr;
            //printf("asm op: %s\n", op);
            switch (op[0]) {
                case ':': addNode(list, PA_create_label(piece)); break;
                case '=': addNode(list, PA_create_equate(piece)); break;
                default:
                    printErrorWithSourceLine("Unknown assembly operation");
            }
        }
    }
    accept("}");
    return createListNode(list);
}