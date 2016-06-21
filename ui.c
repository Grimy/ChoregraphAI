// ui.c - manages terminal input/output
// All output code assumes an ANSI-compatible UTF-8 terminal.

// For display purposes, doors count as walls, but level edges don’t
#define IS_WALL(pos) (TILE(pos).class == WALL && TILE(pos).hp < 5)

static const u8 floor_colors[] = {
	[STAIRS] = 105, [SHOP] = 43,
	[WATER] = 44, [TAR] = 40,
	[FIRE] = 41, [ICE] = 107,
	[OOZE] = 42,
};

// Picks an appropriate box-drawing glyph for a wall by looking at adjacent tiles.
// For example, when tiles to the bottom and right are walls too, use '┌'.
static void display_wall(Coords pos)
{
	switch (TILE(pos).hp) {
	case 0:
		putchar('+');
		return;
	case 2:
		printf(TILE(pos).zone == 2 ? RED : TILE(pos).zone == 3 ? CYAN : "");
		break;
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
	i64 glyph = 0;
	for (i64 i = 0; i < LENGTH(plus_shape); ++i)
		glyph |= IS_WALL(pos + plus_shape[i]) << i;
	printf("%3.3s", &"╳─│┘│┐│┤──└┴┌┬├┼"[3 * glyph]);
}

// Pretty-prints the tile at the given position.
static void display_tile(Coords pos)
{
	Tile *tile = &TILE(pos);
	if (tile->class > FLOOR)
		printf("\033[%um", floor_colors[tile->class]);
	if (tile->monster)
		printf("%s", CLASS(tile->monster).glyph);
	else if (!can_see(pos))
		putchar(' ');
	else if (tile->class == WALL)
		display_wall(pos);
	else
		putchar('.');
	printf(WHITE);
}

// Clears and redraws the entire board.
static void display_board(void)
{
	for (i8 y = 1; y < LENGTH(board) - 1; ++y) {
		for (i8 x = 1; x < LENGTH(*board) - 1; ++x)
			display_tile((Coords) {x, y});
		putchar('\n');
	}

	for (Trap *t = traps; t->pos.x; ++t) {
		if (TILE(t->pos).monster || TILE(t->pos).traps_destroyed)
			continue;
		i64 glyph_index = t->class == BOUNCE ? 15 + 3*t->dir.y + t->dir.x : t->class;
		char *glyph = &"■▫◭◭◆▫⇐⇒◭●●↖↑↗←▫→↙↓↘"[3 * glyph_index];
		printf("\033[%d;%dH%3.3s", t->pos.y, t->pos.x, glyph);
	}

	printf(TERM_HOME);
}

// `play` entry point: alternatively updates the interface and prompts the user for a command.
static void run()
{
	const char *inputs = "efij< z";
	char *p;

	printf(TERM_CLEAR);
	system("stty -echo -icanon eol \1");

	for (;;) {
		display_board();
		p = strchr(inputs, getchar());
		if (p == NULL)
			break;
		do_beat((u8) (p - inputs));
	}

	fprintf(stderr, "See you soon!");
}
