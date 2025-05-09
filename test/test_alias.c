//
// Created by Admin on 5/8/2025.
//


// test simple alias - these should share the same memory location
char x;
alias char y = x;

// test array based alias
char tmpVars[10];

alias char tmp0 = tmpVars[0];
alias char tmp9 = tmpVars[9];


void afunc() {

    // test simple alias - these should share the same memory location
    char a;
    alias char b = a;

    // test array based alias

}

void main() {
    afunc()
}