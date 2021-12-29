// Test code for enumerations

enum GameState { TITLE, MENU, RUNNING, STOPPED }
GameState mainGameState;

#echo "GameState.TITLE",   GameState.TITLE
#echo "GameState.MENU",    GameState.MENU
#echo "GameState.RUNNING", GameState.RUNNING
#echo "GameState.STOPPED", GameState.STOPPED
#echo "Attempt to evaluate GameState without a property reference:", GameState

void test_local_var() {
	GameState localGameState;
	const char RUNNING = 10;
	localGameState = 0;
	localGameState = GameState.RUNNING;
	localGameState = RUNNING;
	
	int i = localGameState;		// this should show a warning
}

void test_switch_case() {
	bool success = false
	switch (mainGameState) {
		case TITLE: {
			#echo "Title case"
			success = true
		}
	}
	int i = 0
	if (success) {
		i = 1
	}
}

void main() {
	mainGameState = 0;
	mainGameState = GameState.RUNNING;
	
	test_local_var()
	
	//mainGameState = TITLE;		// this will fail
	#echo "Attempt to access TITLE without referencing thru GameState:", TITLE
	
	test_switch_case()
}
