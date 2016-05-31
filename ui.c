// ui.c - manages terminal input/output
// All output code assumes an ANSI-compatible UTF-8 terminal.

// ANSI codes to enable color output in the terminal
#define RED    "\033[31m"
#define YELLOW "\033[33m"
#define BLUE   "\033[34m"
#define PINK   "\033[35m"
#define BLACK  "\033[90m"
#define ORANGE "\033[91m"
#define GREEN  "\033[92m"
#define CYAN   "\033[94m"
#define PURPLE "\033[95m"

// For display purposes, doors count as walls, but level edges don’t
#define IS_WALL(y, x) (board[y][x].class == WALL && board[y][x].hp < 5)

static const int floor_colors[] = {[WATER] = 4, [TAR] = 7, [FIRE] = 1, [ICE] = 4, [OOZE] = 2};

// Picks an appropriate box-drawing glyph for a wall by looking at adjacent tiles.
// For example, when tiles to the bottom and right are walls too, use '┌'.
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

// Pretty-prints the tile at the given coordinates.
static void display_tile(long y, long x) {
	Tile e = board[y][x];
	if (e.class > FLOOR)
		printf("\033[4%dm", floor_colors[e.class]);
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

// Clears and redraws the entire board.
static void display_board(void) {
	printf("\033[H\033[2J");
	for (long y = 0; y < LENGTH(board); ++y) {
		for (long x = 0; x < LENGTH(*board); ++x)
			display_tile(y, x);
		putchar('\n');
	}
	for (Trap *t = traps; t->y; ++t) {
		if (board[t->y][t->x].next)
			continue;
		int glyph_index = t->class == BOUNCE ? 15 + 3 * t->dy + t->dx : t->class;
		char *glyph = &"■▫◭◭◆▫⇐⇒◭●●↖↑↗←▫→↙↓↘"[3 * glyph_index];
		printf("\033[%d;%dH%3.3s", t->y + 1, t->x + 1, glyph);
	}
	printf("\033[H");
}

// Updates the interface, then prompts the user for a command.
static void display_prompt() {
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
