// Test code for enumerations

enum GameStates { TITLE, MENU, RUNNING, STOPPED }
GameStates mainGameState;

#echo "GameStates.TITLE",   GameStates.TITLE
#echo "GameStates.MENU",    GameStates.MENU
#echo "GameStates.RUNNING", GameStates.RUNNING
#echo "GameStates.STOPPED", GameStates.STOPPED

void test_local_var() {
	GameStates localGameState;
	const char RUNNING = 10;
	localGameState = 0;
	localGameState = GameStates.RUNNING;
	localGameState = RUNNING;
	
	int i = localGameState;		// this should error out
}

void main() {
	mainGameState = 0;
	mainGameState = TITLE;
	mainGameState = GameStates.RUNNING;
	
	test_local_var()
}
