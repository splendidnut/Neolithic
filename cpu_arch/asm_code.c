//
// Created by admin on 4/22/2020.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "asm_code.h"

struct SMnemonic Mnemonics[] = {
        {"",    MNE_NONE},
        {"ADC", ADC, false},
        {"AND", AND, false},
        {"ASL", ASL, true},
        {"BCC", BCC, false},
        {"BCS", BCS, false},
        {"BEQ", BEQ, false},
        {"BIT", BIT, false},
        {"BMI", BMI, false},
        {"BNE", BNE, false},
        {"BPL", BPL, false},
        {"BRK", BRK, true},
        {"BVC", BVC, false},
        {"BVS", BVS, false},
        {"CLC", CLC, true},
        {"CLD", CLD, true},
        {"CLI", CLI, true},
        {"CLV", CLV, true},
        {"CMP", CMP, false},
        {"CPX", CPX, false},
        {"CPY", CPY, false},
        {"DEC", DEC, false},
        {"DEX", DEX, true},
        {"DEY", DEY, true},
        {"EOR", EOR, false},
        {"INC", INC, false},
        {"INX", INX, true},
        {"INY", INY, true},
        {"JMP", JMP, false},
        {"JSR", JSR, false},
        {"LDA", LDA, false},
        {"LDX", LDX, false},
        {"LDY", LDY, false},
        {"LSR", LSR, true},
        {"NOP", NOP, true},
        {"ORA", ORA, false},
        {"PHA", PHA, true},
        {"PHP", PHP, true},
        {"PLA", PLA, true},
        {"PLP", PLP, true},
        {"ROL", ROL, false},
        {"ROR", ROR, false},
        {"RTI", RTI, true},
        {"RTS", RTS, true},
        {"SBC", SBC, false},
        {"SEC", SEC, true},
        {"SED", SED, true},
        {"SEI", SEI, true},
        {"STA", STA, false},
        {"STX", STX, false},
        {"STY", STY, false},
        {"TAX", TAX, true},
        {"TAY", TAY, true},
        {"TSX", TSX, true},
        {"TXA", TXA, true},
        {"TXS", TXS, true},
        {"TYA", TYA, true},

        // specialty (undocumented) opcodes
        {"DCP", DCP, false},
};

const int NumMnemonics = sizeof(Mnemonics) / sizeof(struct SMnemonic);

enum MnemonicCode lookupMnemonic(char *name) {

    // exit early if name is longer than an opcode
    if (strlen(name) > 4) return MNE_NONE;

    char *lookupName = strdup(name);
    int index = 0;

    // convert to all uppercase (needed by lookup)
    while (lookupName[index] != '\0') {
        lookupName[index] = toupper(lookupName[index]);
        index++;
    }

    // try to find the opcode
    enum MnemonicCode mnemonicCode = MNE_NONE;
    for (index=1; index < NumMnemonics; index++) {
        if (strncmp(lookupName, Mnemonics[index].name, 3) == 0) {
            mnemonicCode = Mnemonics[index].code;
            break;
        }
    }

    free(lookupName);
    return mnemonicCode;
}

char * getMnemonicStr(enum MnemonicCode mneCode) {
    if (Mnemonics[mneCode].code == mneCode) {
        return Mnemonics[mneCode].name;
    } else {
        printf(" ERR: Mnemonics structure mismatch\n");
        return "ERR";
    }
}


struct StAddressMode AddressModes[] = {
        {"",    ADDR_NONE,  1,  ""},
        {"A",   ADDR_ACC,   1,  ""},
        {"IMM", ADDR_IMM,   2,  "#%s"},
        {"ZP",  ADDR_ZP,    2,  "%s"},
        {"ZPX", ADDR_ZPX,   2,  "%s,x"},
        {"ZPY", ADDR_ZPY,   2,  "%s,y"},
        {"ABS", ADDR_ABS,   3,  "%s"},
        {"ABX", ADDR_ABX,   3,  "%s,x"},
        {"ABY", ADDR_ABY,   3,  "%s,y"},
        {"IX",  ADDR_IX,    2,  "(%s,x)"},
        {"IY",  ADDR_IY,    2,  "(%s),y"},
        {"IND", ADDR_IND,   3,  "(%s)"},
        {"REL", ADDR_REL,   2,  "%s"},
};

const int NumAddrModes = sizeof(AddressModes) / sizeof(struct StAddressMode);

enum AddrModes lookupAddrMode(char *name) {
    enum AddrModes addrMode = ADDR_NONE;
    for (int index=1; index < NumAddrModes; index++) {
        if (strncmp(name, AddressModes[index].name, 30) == 0) {
            addrMode = AddressModes[index].mode;
            break;
        }
    }
    return addrMode;
}

struct StAddressMode getAddrModeSt(enum AddrModes addrMode) {
    if (AddressModes[addrMode].mode == addrMode) {
        return AddressModes[addrMode];
    } else {
        printf(" ERR: AddressMode structure mismatch\n");
        return AddressModes[0];
    }
}

int getInstrSize(enum MnemonicCode mne, enum AddrModes addrMode) {
    return ((mne != MNE_NONE) ? AddressModes[addrMode].instrSize : 0);
}

bool isBranch(enum MnemonicCode mne) {
    return ((mne == BEQ) || (mne == BNE) ||
            (mne == BCC) || (mne == BCS) ||
            (mne == BPL) || (mne == BMI) ||
            (mne == BVC) || (mne == BVS));
}