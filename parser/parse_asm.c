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

enum AddrModes getIndirectAddrMode() {
    enum AddrModes addrMode = ADDR_IND;
    switch (peekToken()->tokenType) {
        case TT_CLOSE_PAREN: {   // handle basic (indirect) or (indirect),y
            acceptToken(TT_CLOSE_PAREN);
            if (acceptOptionalToken(TT_COMMA)) {
                char reg = getToken()->tokenStr[0];
                if (reg == 'y' || reg == 'Y') {
                    addrMode = ADDR_IY;
                } else {
                    printError("Expected Y register for indirect indexed addressing mode");
                }
            }
        } break;
        case TT_COMMA: {
            acceptToken(TT_COMMA);
            char reg = getToken()->tokenStr[0];
            if (!(reg == 'x' || reg == 'X')) {
                printError("Expected X register for indexed indirect addressing mode");
            } else {
                addrMode = ADDR_IX;
            }
            acceptToken(TT_CLOSE_PAREN);
        } break;
        default:break;
    }
    return addrMode;
}

enum AddrModes getDirectAddrMode(bool forceAbs) {
    enum AddrModes addrMode = forceAbs ? ADDR_ABS : ADDR_ZP;
    if (acceptOptionalToken(TT_COMMA)) {
        if (inCharset(peekToken(), "XYxy")) {
            char reg = getToken()->tokenStr[0];
            if (reg == 'x' || reg == 'X') {
                addrMode = forceAbs ? ADDR_ABX : ADDR_ZPX;
            } else if (reg == 'y' || reg == 'Y') {
                addrMode = forceAbs ? ADDR_ABY : ADDR_ZPY;
            } else {
                printError("Expected X or Y register for indexed addressing mode");
            }
        } else {
            printError("Expected X or Y register for indexed addressing mode");
        }
    }
    return addrMode;
}


//-----------------------------------------------------------------
//  Parse a single assembler instruction
//
//  [mnemonic]
//  [mnemonic, addressMode, parameter]

ListNode parse_asm_instr(enum MnemonicCode instrCode) {
    List *asmInstr = createList(10);

    addNode(asmInstr, createMnemonicNode(instrCode));

    /*** EXIT if assembly block has ended */
    if (peekToken()->tokenType == TT_CLOSE_BRACE) return createListNode(asmInstr);

    /*** EXIT if we've hit next assembly instruction */
    if (lookupMnemonic(peekToken()->tokenStr) != MNE_NONE) return createListNode(asmInstr);

    /*** EXIT if asm instruction has no parameters */
    if (Mnemonics[instrCode].noParams) return createListNode(asmInstr);

    // process any mnemonic extensions
    bool forceAbs = false;
    if (acceptOptionalToken(TT_PERIOD)) {
        char *mneExt = getToken()->tokenStr;
        if (mneExt[0] == 'w') forceAbs = true;
    }

    // assembly instruction has parameters, so parse them
    enum AddrModes addrMode;
    ListNode paramNode;
    switch (peekToken()->tokenStr[0]) {
        case '#':                   // handle Immediate op
            acceptToken(TT_HASH);
            addrMode = ADDR_IMM;
            paramNode = parse_expr();
            break;
        case '(':                   // handle Indirect op
            acceptToken(TT_OPEN_PAREN);
            paramNode = parse_expr();
            addrMode = getIndirectAddrMode();
            break;
        case '<':
            acceptToken(TT_LESS_THAN);
            // TODO: this should force zeropage mode (currently unnecessary since that's the default)
        default: {
            paramNode = parse_expr();
            if (isBranch(instrCode)) {
                addrMode = ADDR_REL;
            } else {
                addrMode = getDirectAddrMode(forceAbs);
            }
        }
    }
    addNode(asmInstr, createStrNode(getAddrModeSt(addrMode).name));
    addNode(asmInstr, paramNode);
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
    switch (peekToken()->tokenType) {
        case TT_NUMBER:
            addNode(label, createIntNode(copyTokenInt(getToken())));
            break;
        default:
            addNode(label, createStrNode(copyTokenStr(getToken())));
    }
    return createListNode(label);
}

ListNode parse_pseudo_op() {
    char *opName = getToken()->tokenStr;
    if (strncmp(opName, "byte", 4)==0) {
        List *byteData = createList(2);
        addNode(byteData, createParseToken(PT_INIT));
        switch (peekToken()->tokenType) {
            case TT_NUMBER:
                addNode(byteData, createIntNode(copyTokenInt(getToken())));
                break;
            default:
                addNode(byteData, createStrNode(copyTokenStr(getToken())));
        }
        return createListNode(byteData);
    } else {
        printErrorWithSourceLine("Unknown assembly pseudo operation");
    }
}

ListNode parse_asmBlock() {
    List *list = createList(200);
    acceptToken(TT_ASM);
    addNode(list, createParseToken(PT_ASM));

    // get name of asm block (causes it to create a macro)
    if (peekToken()->tokenType != TT_OPEN_BRACE) {
        addNode(list, createStrNode(getToken()->tokenStr));
    } else {
        addNode(list, createEmptyNode());
    }

    // read asm block
    acceptToken(TT_OPEN_BRACE);
    while (peekToken()->tokenType != TT_CLOSE_BRACE) {
        char *piece = copyTokenStr(getToken());
        enum MnemonicCode mnemonicCode = lookupMnemonic(piece);
        if (piece[0] == '.') {
            addNode(list, parse_pseudo_op());
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
    acceptToken(TT_CLOSE_BRACE);
    return createListNode(list);
}