// play.c - manages terminal input/output
// All output code assumes an ANSI-compatible UTF-8 terminal.

#include "chore.h"

#include <time.h>

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
	for (i64 i = 0; i < 4; ++i)
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
		printf("%s", CLASS(&MONSTER(pos)).glyph);
	else if (!TILE(pos).revealed)
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
	for (i8 y = 1; y < ARRAY_SIZE(g.board) - 1; ++y) {
		for (i8 x = 1; x < ARRAY_SIZE(*g.board) - 1; ++x)
			display_tile((Coords) {x, y});
		putchar('\n');
	}

	for (Trap *t = g.traps; t->pos.x; ++t) {
		if (TILE(t->pos).monster || TILE(t->pos).traps_destroyed)
			continue;
		i64 glyph_index = t->class == BOUNCE ? 14 + 3*t->dir.y + t->dir.x : t->class;
		char *glyph = &"■▫◭◭◆▫⇐⇒◭●↖↑↗←▫→↙↓↘"[3 * glyph_index];
		printf("\033[%d;%dH%3.3s", t->pos.y, t->pos.x, glyph);
	}

	printf(TERM_HOME);
}

// `play` entry point: alternatively updates the interface and prompts the user for a command.
int main(i32 argc, char **argv)
{
	xml_parse(argc, argv);
	printf(TERM_CLEAR);
	system("stty -echo -icanon eol \1");
	g.seed = 1;

	while (player.hp > 0 && !player_won()) {
		display_board();
		int c = getchar();
		if (c == EOF)
			break;
		do_beat((u8) c);
	}

	fprintf(stderr, player_won() ? "You won!" : "See you soon!");
}
