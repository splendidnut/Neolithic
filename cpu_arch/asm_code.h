//
// Created by admin on 4/22/2020.
//

#ifndef MODULE_ASM_CODE_H
#define MODULE_ASM_CODE_H

#include <stdbool.h>

enum MnemonicCode {
    MNE_NONE,
    ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC,
    CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP,
    JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI,
    RTS, SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA,

    // specialty ones
    DCP,
};

struct SMnemonic  {
    char *name;
    enum MnemonicCode code;
    bool noParams;              // indicate if opcode is single byte op
};

extern struct SMnemonic Mnemonics[];

enum AddrModes {
    ADDR_NONE = 0,  // none or implied
    ADDR_ACC = 1,
    ADDR_IMM = 2,
    ADDR_ZP = 3,
    ADDR_ZPX = 4,
    ADDR_ZPY = 5,
    ADDR_ABS = 6,
    ADDR_ABX = 7,
    ADDR_ABY = 8,
    ADDR_IX = 9,
    ADDR_IY = 10,
    ADDR_IND = 11,
    ADDR_REL = 12,
};

struct StAddressMode {
    char *name;
    enum AddrModes mode;
    int instrSize;
    char *format;
};

enum ParamExt {
    PARAM_NORMAL,
    PARAM_LO,
    PARAM_HI,
    PARAM_ADD = 0x4,
    PARAM_PLUS_ONE = 0x10   // add (param1 + param2 + 1)  -- special case
};

extern struct StAddressMode AddressModes[];

extern enum MnemonicCode lookupMnemonic(char *name);
extern enum AddrModes lookupAddrMode(char *name);

extern char * getMnemonicStr(enum MnemonicCode mneCode);
extern struct StAddressMode getAddrModeSt(enum AddrModes addrMode);

extern int getInstrSize(enum MnemonicCode mne, enum AddrModes addrMode);

extern bool isBranch(enum MnemonicCode mne);

#endif //MODULE_ASM_CODE_H
