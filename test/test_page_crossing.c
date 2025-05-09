// DO NOT COMPILE WITH OPTIMIZER

int a,b,c,d,e;
char f;

void testBranchToNextPage() {
    a = b + c * 22 * 7;
    b = 7 + c * 21 * 6 * 5;
    c = 7 + c * 21 * 6 * 5;
    e = b+7-c * 5 + d * 2;
    f = 27 + a;
    do {
        f--;
        asm { nop }
    } while (f > 0);
    c = c + 2;
}

void testBranchToPreviousPage() {
    a = b + c * 22 * 7;
    b = 7 + c * 21 * 6 * 5;
    c = 7 + c * 21 * 6 * 5;
    e = b+7-c * 5 + d * 2;
    asm {
        nop
    loop2:
        nop
        dey
        bne loop2  // x
    }
    c = c + 2;
}

void main() {
    testBranchToNextPage();
    testBranchToPreviousPage();
}