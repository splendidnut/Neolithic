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

//
// Created by admin on 4/23/2020.
//

#include <string.h>
#include <stdbool.h>

#include "cpu_arch/asm_code.h"

#include "parse_asm.h"
#include "parser.h"

#define MAX_ASSEMBLY_STATEMENTS 4000

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

enum AddrModes absAddrModes[3] = {ADDR_ABS, ADDR_ABX, ADDR_ABY};
enum AddrModes zpAddrModes[3] = {ADDR_ZP, ADDR_ZPX, ADDR_ZPY};
enum AddrModes unkAddrModes[3] = {ADDR_UNK_M, ADDR_UNK_MX, ADDR_UNK_MY};

enum AddrModes getDirectAddrMode(bool forceZp, bool forceAbs) {
    enum AddrModes *addrModeSet;

    if (forceZp) {
        addrModeSet = zpAddrModes;
    } else if (forceAbs) {
        addrModeSet = absAddrModes;
    } else {
        addrModeSet = unkAddrModes;
    }

    enum AddrModes addrMode = addrModeSet[0];//forceAbs ? ADDR_ABS : ADDR_ZP;
    if (acceptOptionalToken(TT_COMMA)) {
        if (inCharset(peekToken(), "XYxy")) {
            char reg = getToken()->tokenStr[0];
            if (reg == 'x' || reg == 'X') {
                addrMode = addrModeSet[1];//forceAbs ? ADDR_ABX : ADDR_ZPX;
            } else if (reg == 'y' || reg == 'Y') {
                addrMode = addrModeSet[2];//forceAbs ? ADDR_ABY : ADDR_ZPY;
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
    bool forceZp = false;
    if (acceptOptionalToken(TT_PERIOD)) {
        char *mneExt = getToken()->tokenStr;
        if (mneExt[0] == 'w') forceAbs = true;
        if (mneExt[0] == 'z') forceZp = true;
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
            forceZp = true;             // TODO: this should force zeropage mode (currently unnecessary since that's the default)
        default: {
            // branches typically just use labels... to use expressions, they need to be wrapped in parenthesis
            //   NOTE: branches and jumps are done separately because of the possibility of '.byte' being
            //          parsed as a struct property... TODO: Figure out how to fix that
            if (isBranch(instrCode)) {
                if (peekToken()->tokenStr[0] == '(') {
                    paramNode = parse_expr();
                } else {
                    paramNode = parse_identifier();
                }
                addrMode = ADDR_REL;
            } else if (instrCode == JMP || instrCode == JSR) {
                if (peekToken()->tokenStr[0] == '(') {
                    paramNode = parse_expr();
                    addrMode = ADDR_IND;
                } else {
                    paramNode = parse_identifier();
                }
                addrMode = ADDR_ABS;
            } else {
                paramNode = parse_expr();
                addrMode = getDirectAddrMode(forceZp, forceAbs);
            }
        }
    }
    //addNode(asmInstr, createStrNode(getAddrModeSt(addrMode).name));
    addNode(asmInstr, createAddrModeNode(addrMode));
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
            //addNode(label, createStrNode(copyTokenStr(getToken())));
            addNode(label, parse_expr());
    }
    return createListNode(label);
}

ListNode parse_pseudo_op() {
    char *opName = getToken()->tokenStr;
    bool isByte = (strncmp(opName, "byte", 4)==0);
    bool isWord = (strncmp(opName, "word", 4)==0);
    if (isByte || isWord) {
        List *byteData = createList(3);
        addNode(byteData, createParseToken(PT_INIT));
        addNode(byteData, createParseToken(isWord ? PT_WORD : PT_BYTE));
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
        return createEmptyNode();
    }
}


ListNode parse_asmBlock() {
    List *list = createList(MAX_ASSEMBLY_STATEMENTS);
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

    list = condenseList(list);
    return createListNode(list);
}