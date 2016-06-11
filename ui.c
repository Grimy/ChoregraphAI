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
#define IS_WALL(tile) ((tile)->class == WALL && (tile)->hp < 5)

static const int floor_colors[] = {[SHOP] = 43, [WATER] = 44, [FIRE] = 41, [ICE] = 107, [OOZE] = 42};

// Picks an appropriate box-drawing glyph for a wall by looking at adjacent tiles.
// For example, when tiles to the bottom and right are walls too, use '┌'.
static void display_wall(Tile *wall) {
	switch (wall->hp) {
	case 0:
		putchar('+');
		return;
	case 3:
		printf(BLACK);
		break;
	case 4:
		printf(YELLOW);
		break;
	case 5:
		putchar(' ');
		return;
	}
	long glyph = 0;
	for (PLUS_SHAPE(wall))
		glyph = (glyph << 1) | (wall->class == WALL && wall->hp < 5);
	printf("%3.3s", &"╳───│┌┐┬│└┘┴│├┤┼"[3 * (glyph & 15)]);
}

// Pretty-prints the tile at the given position.
static void display_tile(Coords pos) {
	Tile *tile = &TILE(pos);
	if (tile->class > FLOOR)
		printf("\033[%dm", floor_colors[tile->class]);
	if (tile->monster)
		printf("%s", CLASS(tile->monster).glyph);
	else if (!can_see(pos))
		putchar(' ');
	else if (tile->class == WALL)
		display_wall(tile);
	else
		putchar('.');
	printf("\033[m");
}

// Clears and redraws the entire board.
static void display_board(void) {
	printf("\033[H\033[2J");
	for (int8_t y = 1; y < LENGTH(board) - 1; ++y) {
		for (int8_t x = 1; x < LENGTH(*board) - 1; ++x)
			display_tile((Coords) {x, y});
		putchar('\n');
	}
	for (Trap *t = traps; t->pos.x; ++t) {
		if (TILE(t->pos).monster)
			continue;
		int glyph_index = t->class == BOUNCE ? 15 + 3*t->dir.y + t->dir.x : t->class;
		char *glyph = &"■▫◭◭◆▫⇐⇒◭●●↖↑↗←▫→↙↓↘"[3 * glyph_index];
		printf("\033[%d;%dH%3.3s", t->pos.y, t->pos.x, glyph);
	}
	printf("\033[H");
}

// Updates the interface, then prompts the user for a command.
static char display_prompt() {
	// static long turn = 0;
	// if (++turn >= 1000000)
		// exit(0);
	// if (turn)
		// return;
	display_board();
	return (char) (getchar());
}
