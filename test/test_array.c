//--- Test array functionality
char a,b,i;
char array[8];

const byte SIZE = 7;
char arraySizeTest[SIZE];
const char arraySizeTest2[SIZE - 1] = {1,2,3,4,5,6};


void fill_array() {
	for (i=0; i<8; i++) {
		array[i] = i;
	}
}

void fill_array_with_loop() {
	loop (i, 0, 8) {
		array[i] = i;
	}
}

/**
 *  Perform tests with arrays using only a constant index
 */
void test_array_with_const_index() {
	a = 5;
	b = 3;
	array[0] = 0;
		
	//-- OPs on array elements
	array[0] = 3 + 6;
	array[1] = a + 2;
	array[2] = 3 * b;
	array[3] = a - b;
	array[4] = array[0] + 6;
	array[5] = array[b] + a;
	array[6] = array[a] + array[b];
	array[7]++;

    //--- read from array using a variety of indexes
    a = array[0];
    a = array[b];
    a = array[b+5];
    a = array[b-5];
    a = array[a+b];
}

/**
 *  Perform tests with arrays using index variables 
 */
void test_array_with_var_index() {
	
	// quick test var index
	a = 3;
	array[a] = 7;
	
	// test basic arithmetic
	a = 6;
	b = 5;
	array[b] = array[a] + 13;
	
	// test INC operator
	i = 0;
	do {
		array[i]++;
		i++;
	} while (i < 8);
	
	//--- test index var arithmetic
	array[b + 1] = array[a + 1] + 3;
    array[b - 1] = array[a - 1] - 2;
	//array[a - b] = 6;
}

void main() {
	array[0] = 0;		// quick basic test
	
	fill_array();
	test_array_with_const_index();
	
	fill_array_with_loop();
	test_array_with_var_index();
}