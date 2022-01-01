
struct GameObject {
    int x,y;
    char timer;
};

#echo "Size of GameObject", sizeof(GameObject)

struct GameObject2 {
	char x,y,z;
}

#echo "Size of GameObject2", sizeof(GameObject2)

const GameObject2 origin = {0,0,0}

void main() {
	const GameObject enemy = {0,0,0,0}	// too many values
}
