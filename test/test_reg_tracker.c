char a,b,c,d,e,f;
void main() {
    a = 0;      // A reg is loaded with 0
    b = 0;      // ...so shouldn't need to be reloaded
    c = 0;
    d = 0;
    e = 10;     // until here... where it's loaded with a different value
    f = 0;

    a = 10;     // A reg is loaded with 10... and stored... so both '10' and 'a' exist in A reg.
    b = a;      // A reg WAS STORED WITH VAR 'a',
    c = a;

    // TODO: Add more complicated cases than the following to check.
    a = 5;
    b = a + 2;
    c = a;
}