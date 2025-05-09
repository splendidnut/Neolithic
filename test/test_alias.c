//
// Created by Admin on 5/8/2025.
//


// test simple alias - these should share the same memory location
char x,y;
alias char x2 = x;
alias int xy = x;

// test pointer with alias to low/high bytes
char *gfxPtr;
alias char pl = gfxPtr;
alias char ph = pl+1;

// test array based alias
char tmpVars[10];
alias char tmp0 = tmpVars[0];
alias char tmp9 = tmpVars[9];

// test struct based alias
struct abStruct {
    char a,b;
};
abStruct ab;
//alias char d = ab.a;

void afunc() {

    // test simple alias - these should share the same memory location
    char a;
    alias char b = a;

    // test array based alias
    byte c = a + b
}

void main() {
    afunc()
}