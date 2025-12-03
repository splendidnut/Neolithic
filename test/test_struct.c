
struct GameObject {
    int x,y;
    char timer;
};

#echo "Size of GameObject", sizeof(GameObject)

struct GameObject2 {
	char x,y,z;
}

#echo "Size of GameObject2", sizeof(GameObject2)

const GameObject2 origin = {0,0,0};

GameObject objs[2]

void main() {
    objs[0].x = 7;
    objs[0].y = 0;
    objs[1].timer = 0x80;
    objs[0].timer = 0x40;
    char curTimer = objs[1].timer;
	const GameObject enemy = {0,0,0,0};	// too many values
}
