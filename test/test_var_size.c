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
Seven *ptrToSeven[2]    // 4 bytes  - two pointers to structs

char lastVar

//--- above totals 33 bytes
#echo "Test Variable Sizes"
#echo ""
#echo "Address of sevenByteStruct = ", &sevenByteStruct, " should be 138"
#echo ""
#echo "Variable space used      = ", (&lastVar - 128)
#echo "Size used for basic vars = ", (&sevenByteStruct - 128), " should be 10 (2 bytes used by accumulator)"
#echo "Sizeof (struct Seven)    = ", sizeof(Seven), " should be 7"
#echo "Sizeof (var fourteen)    = ", sizeof(fourteen), " should be 14"
#echo "Sizeof (var ptrToSeven)  = ", sizeof(ptrToSeven), " should be 4"
#echo ""
#echo "Address of Last variable = ", &lastVar
#echo "Variable space used      = ", (&lastVar - 128)

void main() {}