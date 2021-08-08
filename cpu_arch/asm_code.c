//
// Created by admin on 4/22/2020.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "common/common.h"
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
    int i = 0;

    // convert to all uppercase (needed by lookup)
    while (lookupName[i] != '\0') {
        lookupName[i] = toupper(lookupName[i]);
        i++;
    }

    // try to find the opcode
    enum MnemonicCode mnemonicCode = MNE_NONE;
    for_range (index, 1, NumMnemonics) {
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
        {"",    ADDR_NONE,  1, 0,  ""},         // NOTE: cycles = 0, since dependent on instruction
        {"A",   ADDR_ACC,   1, 2,  ""},
        {"IMM", ADDR_IMM,   2, 2,  "#%s"},
        {"ZP",  ADDR_ZP,    2, 0,  "%s"},
        {"ZPX", ADDR_ZPX,   2, 0,  "%s,x"},
        {"ZPY", ADDR_ZPY,   2, 0,  "%s,y"},
        {"ABS", ADDR_ABS,   3, 0,  "%s"},
        {"ABX", ADDR_ABX,   3, 0,  "%s,x"},
        {"ABY", ADDR_ABY,   3, 0,  "%s,y"},
        {"IX",  ADDR_IX,    2, 0,  "(%s,x)"},
        {"IY",  ADDR_IY,    2, 0,  "(%s),y"},
        {"IND", ADDR_IND,   3, 0,  "(%s)"},
        {"REL", ADDR_REL,   2, 2,  "%s"},       // SPECIAL CASE: cycles can be 2 or 3
};

const int NumAddrModes = sizeof(AddressModes) / sizeof(struct StAddressMode);

enum AddrModes lookupAddrMode(char *name) {
    enum AddrModes addrMode = ADDR_NONE;
    for_range (index, 1, NumAddrModes) {
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


struct StOpcodeTable opcodeTable[] = {
        {MNE_NONE, ADDR_NONE, 0, 0},
        //
        {ADC, ADDR_IMM, 0x69, 2},
        {ADC, ADDR_ZP,  0x65, 3},
        {ADC, ADDR_ZPX, 0x75, 4},
        {ADC, ADDR_ABS, 0x6D, 4},
        {ADC, ADDR_ABX, 0x7D, 4},
        {ADC, ADDR_ABY, 0x79, 4},
        {ADC, ADDR_IX,  0x61, 6},
        {ADC, ADDR_IY,  0x71, 5},
        //
        {AND, ADDR_IMM, 0x29, 2},
        {AND, ADDR_ZP,  0x25, 3},
        {AND, ADDR_ZPX, 0x35, 4},
        {AND, ADDR_ABS, 0x2D, 4},
        {AND, ADDR_ABX, 0x3D, 4},
        {AND, ADDR_ABY, 0x39, 4},
        {AND, ADDR_IX,  0x21, 6},
        {AND, ADDR_IY,  0x31, 5},
        //
        {ASL, ADDR_NONE, 0x0A, 2},
        {ASL, ADDR_ACC, 0x0A, 2},
        {ASL, ADDR_ZP,  0x06, 5},
        {ASL, ADDR_ZPX, 0x16, 6},
        {ASL, ADDR_ABS, 0x0E, 6},
        {ASL, ADDR_ABX, 0x1E, 7},
        //
        {BIT, ADDR_ZP,  0x24, 3},
        {BIT, ADDR_ABS, 0x2C, 4},
        //
        {BPL, ADDR_REL, 0x10, 2},
        {BMI, ADDR_REL, 0x30, 2},
        {BVC, ADDR_REL, 0x50, 2},
        {BVS, ADDR_REL, 0x70, 2},
        {BCC, ADDR_REL, 0x90, 2},
        {BCS, ADDR_REL, 0xB0, 2},
        {BNE, ADDR_REL, 0xD0, 2},
        {BEQ, ADDR_REL, 0xF0, 2},
        //
        {BRK, ADDR_NONE,0x00, 7},
        //
        {CLC, ADDR_NONE,0x18, 2},
        {CLD, ADDR_NONE,0xD8, 2},
        {CLI, ADDR_NONE,0x58, 2},
        {CLV, ADDR_NONE,0xB8, 2},
        //
        {CMP, ADDR_IMM, 0xC9, 2},
        {CMP, ADDR_ZP,  0xC5, 3},
        {CMP, ADDR_ZPX, 0xD5, 4},
        {CMP, ADDR_ABS, 0xCD, 4},
        {CMP, ADDR_ABX, 0xDD, 4},
        {CMP, ADDR_ABY, 0xD9, 4},
        {CMP, ADDR_IX,  0xC1, 6},
        {CMP, ADDR_IY,  0xD1, 5},
        //
        {CPX, ADDR_IMM, 0xE0, 2},
        {CPX, ADDR_ZP,  0xE4, 3},
        {CPX, ADDR_ABS, 0xEC, 4},
        //
        {CPY, ADDR_IMM, 0xC0, 2},
        {CPY, ADDR_ZP,  0xC4, 3},
        {CPY, ADDR_ABS, 0xCC, 4},
        //
        {DCP, ADDR_ZP,  0xC7, 5},
        {DCP, ADDR_ZPX, 0xD7, 6},
        {DCP, ADDR_ABS, 0xCF, 6},
        {DCP, ADDR_ABX, 0xDF, 7},
        {DCP, ADDR_ABY, 0xDB, 7},
        {DCP, ADDR_IX,  0xC3, 8},
        {DCP, ADDR_IY,  0xD3, 8},
        //
        {DEC, ADDR_ZP,  0xC6, 5},
        {DEC, ADDR_ZPX, 0xD6, 6},
        {DEC, ADDR_ABS, 0xCE, 6},
        {DEC, ADDR_ABX, 0xDE, 7},
        //
        {DEX, ADDR_NONE,0xCA, 2},
        {DEY, ADDR_NONE,0x88, 2},
        //
        {EOR, ADDR_IMM, 0x49, 2},
        {EOR, ADDR_ZP,  0x45, 3},
        {EOR, ADDR_ZPX, 0x55, 4},
        {EOR, ADDR_ABS, 0x4D, 4},
        {EOR, ADDR_ABX, 0x5D, 4},
        {EOR, ADDR_ABY, 0x59, 4},
        {EOR, ADDR_IX,  0x41, 6},
        {EOR, ADDR_IY,  0x51, 5},
        //
        {INC, ADDR_ZP,  0xE6, 5},
        {INC, ADDR_ZPX, 0xF6, 6},
        {INC, ADDR_ABS, 0xEE, 6},
        {INC, ADDR_ABX, 0xFE, 7},
        //
        {INX, ADDR_NONE,0xE8, 2},
        {INY, ADDR_NONE,0xC8, 2},
        //
        {JMP, ADDR_ABS, 0x4C, 3},
        {JMP, ADDR_IND, 0x6C, 5},
        {JSR, ADDR_ABS, 0x20, 6},
        //
        {LDA, ADDR_IMM, 0xA9, 2},
        {LDA, ADDR_ZP,  0xA5, 3},
        {LDA, ADDR_ZPX, 0xB5, 4},
        {LDA, ADDR_ABS, 0xAD, 4},
        {LDA, ADDR_ABX, 0xBD, 4},
        {LDA, ADDR_ABY, 0xB9, 4},
        {LDA, ADDR_IX,  0xA1, 6},
        {LDA, ADDR_IY,  0xB1, 5},
        //
        {LDX, ADDR_IMM, 0xA2, 2},
        {LDX, ADDR_ZP,  0xA6, 3},
        {LDX, ADDR_ZPY, 0xB6, 4},
        {LDX, ADDR_ABS, 0xAE, 4},
        {LDX, ADDR_ABY, 0xBE, 4},
        //
        {LDY, ADDR_IMM, 0xA0, 2},
        {LDY, ADDR_ZP,  0xA4, 3},
        {LDY, ADDR_ZPX, 0xB4, 4},
        {LDY, ADDR_ABS, 0xAC, 4},
        {LDY, ADDR_ABX, 0xBC, 4},
        //
        {LSR, ADDR_NONE, 0x4A, 2},
        {LSR, ADDR_ACC, 0x4A, 2},
        {LSR, ADDR_ZP,  0x46, 5},
        {LSR, ADDR_ZPX, 0x56, 6},
        {LSR, ADDR_ABS, 0x4E, 6},
        {LSR, ADDR_ABX, 0x5E, 7},
        //
        {NOP, ADDR_NONE,0xEA, 2},
        //
        {ORA, ADDR_IMM, 0x09, 2},
        {ORA, ADDR_ZP,  0x05, 3},
        {ORA, ADDR_ZPX, 0x15, 4},
        {ORA, ADDR_ABS, 0x0D, 4},
        {ORA, ADDR_ABX, 0x1D, 4},
        {ORA, ADDR_ABY, 0x19, 4},
        {ORA, ADDR_IX,  0x01, 6},
        {ORA, ADDR_IY,  0x11, 5},
        //
        {PHA, ADDR_NONE,0x48, 3},
        {PHP, ADDR_NONE,0x08, 3},
        {PLA, ADDR_NONE,0x68, 4},
        {PLP, ADDR_NONE,0x28, 4},
        //
        {ROL, ADDR_NONE, 0x2A, 2},
        {ROL, ADDR_ACC, 0x2A, 2},
        {ROL, ADDR_ZP,  0x26, 5},
        {ROL, ADDR_ZPX, 0x36, 6},
        {ROL, ADDR_ABS, 0x2E, 6},
        {ROL, ADDR_ABX, 0x3E, 7},
        //
        {ROR, ADDR_NONE, 0x6A, 2},
        {ROR, ADDR_ACC, 0x6A, 2},
        {ROR, ADDR_ZP,  0x66, 5},
        {ROR, ADDR_ZPX, 0x76, 6},
        {ROR, ADDR_ABS, 0x6E, 6},
        {ROR, ADDR_ABX, 0x7E, 7},
        //
        {RTI, ADDR_NONE,0x40, 6},
        {RTS, ADDR_NONE,0x60, 6},
        //
        {SBC, ADDR_IMM, 0xE9, 2},
        {SBC, ADDR_ZP,  0xE5, 3},
        {SBC, ADDR_ZPX, 0xF5, 4},
        {SBC, ADDR_ABS, 0xED, 4},
        {SBC, ADDR_ABX, 0xFD, 4},
        {SBC, ADDR_ABY, 0xF9, 4},
        {SBC, ADDR_IX,  0xE1, 6},
        {SBC, ADDR_IY,  0xF1, 5},
        //
        {SEC, ADDR_NONE,0x38, 2},
        {SED, ADDR_NONE,0xF8, 2},
        {SEI, ADDR_NONE,0x78, 2},
        //
        {STA, ADDR_ZP,  0x85, 3},
        {STA, ADDR_ZPX, 0x95, 4},
        {STA, ADDR_ABS, 0x8D, 4},
        {STA, ADDR_ABX, 0x9D, 5},
        {STA, ADDR_ABY, 0x99, 5},
        {STA, ADDR_IX,  0x81, 6},
        {STA, ADDR_IY,  0x91, 6},
        //
        {STX, ADDR_ZP,  0x86, 3},
        {STX, ADDR_ZPX, 0x96, 4},
        {STX, ADDR_ABS, 0x8E, 4},
        //
        {STY, ADDR_ZP,  0x84, 3},
        {STY, ADDR_ZPX, 0x94, 4},
        {STY, ADDR_ABS, 0x8C, 4},
        //
        {TAX, ADDR_NONE,0xAA, 2},
        {TAY, ADDR_NONE,0xA8, 2},
        {TSX, ADDR_NONE,0xBA, 2},
        {TXA, ADDR_NONE,0x8A, 2},
        {TXS, ADDR_NONE,0x9A, 2},
        {TYA, ADDR_NONE,0x98, 2},
};

const int NumOpcodes = sizeof(opcodeTable) / sizeof(struct StOpcodeTable);

OpcodeEntry lookupOpcodeEntry(enum MnemonicCode mneCode, enum AddrModes addrMode) {
    OpcodeEntry opcodeEntry = opcodeTable[0];
    for_range(index, 1, NumOpcodes) {
        if ((opcodeTable[index].mneCode == mneCode) &&
                (opcodeTable[index].addrMode == addrMode)) {
            opcodeEntry = opcodeTable[index];
            break;
        }
    }
    return opcodeEntry;
}

int getCycleCount(enum MnemonicCode mne, enum AddrModes addrMode) {
    OpcodeEntry opcodeEntry = lookupOpcodeEntry(mne, addrMode);

    // need to fix issue with non-existent ZPY modes (switch to ABY)
    if ((opcodeEntry.mneCode == MNE_NONE) && (addrMode == ADDR_ZPY)) {
        addrMode = ADDR_ABY;
        opcodeEntry = lookupOpcodeEntry(mne, addrMode);
    }

    if (opcodeEntry.mneCode != MNE_NONE) {
        return opcodeEntry.cycles;
    }
    return 0;
}
