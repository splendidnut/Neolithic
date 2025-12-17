// Test pointers and all the lovely things that can be done with them
// Created on 12/13/2025.
//

char *basicDataPtr;
char *pointerArray[7];   //-- an array of 7 pointers to 8-bit data

const char romData0[] = { 0, 1, 2, 3, 4, 5, 6, 7};
const char romData1[] = { 0, 1, 2, 3, 4, 5, 6, 7};

const char* romDataPtrs[] = { &romData0, &romData1 }

void main() {
    char x = 1;
    char y = 0;

    basicDataPtr = $2000;   /// set pointer address (16-bit LDA/STA combo)

    //*basicDataPtr = 55;     // store data via pointer access

    /// store data via array access
    basicDataPtr[0] = 76;
    basicDataPtr[1] = 55;

    // copy pointer addr from array into pointer
    basicDataPtr = romDataPtrs[0];
    basicDataPtr = romDataPtrs[x];

    x = 2;

    // copy address from one pointer into an array of pointers (16-bit data)
    pointerArray[0] = basicDataPtr;    //-- copy pointer
    pointerArray[1] = &basicDataPtr;   //-- copy address of var containing pointer
    pointerArray[x] = basicDataPtr;

    //-- copy pointer from one array to another
    pointerArray[2] = romDataPtrs[x];
    pointerArray[x] = romDataPtrs[y];

    //--- tricky:
    //*pointerArray[2] = x;       //-- save data to one of the pointer addresses.
}