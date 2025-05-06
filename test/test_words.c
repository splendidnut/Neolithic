//  Test word size operations
/*
 *  BTW: Another bug: Something like 'word = word | another word' causes only the low bytes
 *      to be ORed but the high byte replaced with >another word.
 *
 *  And another one: Reading from a byte table and addressing to a word seems not handled
 *  well at all: the index is doubled as if there are words in the table and X is stored
 *  into the high bytes without being defined at all.
 *
 *  So overall addressing the bytes of a word seems not considered yet. But it will save space.
 */

const int b = 7

int a

const byte addr1[] = {1,2,3,4};
const byte addr2[] = {1,2,3,4};
const byte addr3[] = {1,2,3,4};

void split_pointer_op() {
    byte index;
    int pointer;
    asm {
        pointerTable:
    .byte <addr1, <addr2, <addr3
    //.byte 3

    ldy index
    lda pointerTable, y
    sta pointer
    lda #>addr1
    sta
    pointer + 1
    }
}

void main() {
    a = 2776;
    a++;

    char al = <a;
    char ah = >a;

    int c = a+b;

    int d = a*b;

    int e = a | b;

    int f = a & b;

    int g = a << 4;

    split_pointer_op();
}

