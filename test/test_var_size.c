// this tests the variable sizes
bool b      // 1 byte
char c      // 1 byte
int i       // 2 bytes

struct Seven {
    char a,b,c
    int d,e
};

Seven sevenByteStruct   // 7 bytes
Seven fourteen[2]       // 14 bytes
Seven *x[2]             // 4 bytes

char lastVar

//--- above totals 30 bytes