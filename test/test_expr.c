char x,y,z;
int a,b,c;

//--------------------------
//  Test 8-bit expressions

void test8() {
    x = 1;
    y = x + 1;
    z = y - x;
    z = x * 5;
}

//-----------------------------------------
//  Test 8/16-bit mixed expressions

void testMixed816() {
    a = 1;
    b = a + x;
    c = b - y;
}

void testMul8() {
	c = 2 * 3	// test eval
	
	c = x * 7	// test var/const symetry
	c = 7 * x
	
	c = x * y	// test var/var
	
	c = (x + y) * 2
	c = (x + y) * x
}

//--------------------------
//  Test 16-bit expressions

void test16() {
    a = 1;
    b = 2;

    c = a + b;
    c = a - b;
}

//---------------------------------------
//  Test complicated 8-bit expressions

void testComplex8bit() {
	x = 4
	y = 3
	z = (x * 2) + y
	z = (x * 2) + (y * 3)
	
	y = 7
	z = (x * 2) + ((y-7) * 3)
}

void NEED_TO_FIX_testComplex8bit() {
	x = 5
	y = 7
	z = (x + 2) * y
}

//=============================

void main() {
    test8();
    testMixed816();
    test16();
	
	testMul8();
	
	testComplex8bit();
}