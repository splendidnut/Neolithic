const char HEIGHT = 12;
const char OTHER_HEIGHT = HEIGHT;
const char TOTAL_HEIGHT = HEIGHT + OTHER_HEIGHT;

void main() {
    char a = HEIGHT;
    char b = OTHER_HEIGHT;
    char c = TOTAL_HEIGHT;
    char d = DEFINED_LATE;
    char e = lateDataArray[2];
    char x = 1;
    char f = lateDataArray[x];
    char g = lateDataArray[x+2];

    asm {
        lda  lateDataArray+1,y
        sta  a
    }
}


const char DEFINED_LATE = 12;
const char lateDataArray[] = {12,24,36,48};