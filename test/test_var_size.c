// this tests the basic variable sizes  (8 bytes)
bool b      // 1 byte
char c      // 1 byte
int i       // 2 bytes
char *x     // 2 bytes - pointer to char(s)
int  *y     // 2 bytes - pointer to int(s)

// test structure   (variables should take 25 bytes of space)
struct Seven {
    char a,b,c
    int d,e
};

Seven sevenByteStruct   // 7 bytes  - single struct
Seven fourteen[2]       // 14 bytes - two structs
Seven *x[2]             // 4 bytes  - two pointers to structs

char lastVar

//--- above totals 33 bytes