#define RED    "\033[31m"
#define YELLOW "\033[33m"
#define BLUE   "\033[34m"
#define PURPLE "\033[35m"
#define BLACK  "\033[90m"
#define GREEN  "\033[92m"
#define ORANGE "\033[93m"

#define IS_WALL(y, x) (board[y][x].class == WALL && board[y][x].hp < 5)

static void display_wall(long y, long x) {
	if (board[y][x].hp == 0) {
		putchar('+');
		return;
	}
	if (board[y][x].hp == 3)
		printf(BLACK);
	long glyph =
		IS_WALL(y - 1, x) << 3 |
		IS_WALL(y + 1, x) << 2 |
		IS_WALL(y, x - 1) << 1 |
		IS_WALL(y, x + 1);
	printf("%3.3s", &"╳───│┌┐┬│└┘┴│├┤┼"[3*glyph]);
}

static void display_tile(long y, long x) {
	Tile e = board[y][x];
	if (e.class == OOZE)
		printf("\033[42m");
	if (e.next)
		printf("%s", CLASS(e.next).glyph);
	else if (e.class == WALL && e.hp == 5)
		putchar(' ');
	else if (!can_see(y, x))
		putchar(' ');
	else if (e.class == WALL)
		display_wall(y, x);
	else
		putchar('.');
	printf("\033[m");
}

static void display_board(void) {
	printf("\033[H\033[2J");
	for (long y = 0; y < LENGTH(board); ++y) {
		for (long x = 0; x < LENGTH(*board); ++x)
			display_tile(y, x);
		putchar('\n');
	}
	for (Trap *t = traps; t->y; ++t)
		printf("\033[%d;%dH^", t->y + 1, t->x + 1);
}

static void player_turn() {
	// static long turn = 0;
	// if (++turn >= 1000000)
		// exit(0);
	// if (turn)
		// return;
	display_board();
	switch (getchar()) {
		case 'e': player_move( 0, -1); break;
		case 'f': player_move( 1,  0); break;
		case 'i': player_move( 0,  1); break;
		case 'j': player_move(-1,  0); break;
		case 't': player.hp = 0; break;
		default: break;
	}
}
