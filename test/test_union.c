
// anonymous union is broken
union {
    char a,b
    int i
};

// BUT, named union works
union myUnion {
    char a,b
    int i
};

void main() {
  char c = a+b
  //char d = myUnion.a + myUnion.b   //--currently crashes the compiler
}