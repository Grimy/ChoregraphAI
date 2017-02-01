// play.c - manages terminal input/output
// All output code assumes an ANSI-compatible UTF-8 terminal.

#include "chore.h"

#include <time.h>
#include <unistd.h>

static const u8 floor_colors[] = {
	[STAIRS] = 105, [SHOP_FLOOR] = 43,
	[WATER] = 44, [TAR] = 47,
	[FIRE] = 41, [ICE] = 107,
	[OOZE] = 42, [WIRE] = 0,
};

// Picks an appropriate box-drawing glyph for a wall by looking at adjacent tiles.
// For example, when tiles to the bottom and right are walls too, use '┌'.
static void display_wall(Coords pos)
{
	if (TILE(pos).class == FIREWALL)
		printf(RED);
	else if (TILE(pos).class == ICEWALL)
		printf(CYAN);
	else if (TILE(pos).hp == 3)
		printf(BLACK);
	else if (TILE(pos).hp == 4)
		printf(YELLOW);

	i64 glyph = 0;
	for (i64 i = 0; i < 4; ++i)
		glyph |= IS_DIGGABLE(pos + plus_shape[i]) << i;
	printf("%3.3s", &"╳─│┘│┐│┤──└┴┌┬├┼"[3 * glyph]);
}

static void display_wire(Coords pos)
{
	i64 glyph = 0;
	for (i64 i = 0; i < 4; ++i)
		glyph |= (TILE(pos + plus_shape[i]).wired) << i;
	printf(YELLOW "%3.3s", &"⋅╴╵╯╷╮│┤╶─╰┴╭┬├┼"[3 * glyph]);
}

// Pretty-prints the tile at the given position.
static void display_tile(Coords pos)
{
	Tile *tile = &TILE(pos);

	if (tile->class == EDGE || !tile->revealed)
		return;

	cursor_to(pos.x, pos.y);
	if (!BLOCKS_LOS(pos))
		printf("\033[%um", floor_colors[tile->class]);

	if (tile->monster)
		printf("%s", CLASS(&MONSTER(pos)).glyph);
	else if (IS_DOOR(pos))
		putchar('+');
	else if (IS_DIGGABLE(pos))
		display_wall(pos);
	else if (IS_WIRE(pos))
		display_wire(pos);
	else
		putchar(tile->item ? '*' : '.');

	printf(WHITE);
}

#define LINE(fmt, ...) printf("\033[u\033[B\033[s " fmt, __VA_ARGS__)

static void display_inventory(void)
{
	cursor_to(32, 0);
	printf("\033[s");
	LINE("%s (%d, %d)", "Bard", player.pos.x, player.pos.y);
	LINE("HP: %.1f/%d", player.hp / 2.0, 2);
	LINE("Bombs: %d", g.inventory[BOMBS]);
	LINE("Weapon: %s", g.inventory[JEWELED] ? "Jeweled Dagger" : "Dagger");
	if (g.inventory[LUNGING])
		LINE("Boots: Lunging (%s)", g.boots_on ? "on" : "off");
	if (g.inventory[MEMERS_CAP])
		LINE("Headpiece: %s", "Miner’s Cap");
}

static void display_enemy(Monster *m)
{
	if (m->hp <= 0)
		return;
	LINE("%s" WHITE " (%2d, %2d) (%2d, %2d): %dhp%s%s%s",
		CLASS(m).glyph,
		m->pos.x,
		m->pos.y,
		m->prev_pos.x,
		m->prev_pos.y,
		m->hp,
		m->aggro ? ", aggroed" : "",
		m->freeze > g.current_beat ? ", frozen" : "",
		m->confused ? ", confused" : "");
}

static void display_trap(Trap *t)
{
	if (!TILE(t->pos).revealed || TILE(t->pos).destroyed)
		return;
	i64 glyph_index = t->class == BOUNCE ? 14 + 3*t->dir.y + t->dir.x : t->class;
	cursor_to(t->pos.x, t->pos.y);
	printf("%3.3s", &"■▫◭◭◆▫⇐⇒◭●↖↑↗←▫→↙↓↘"[3 * glyph_index]);
}

// Clears and redraws the entire interface.
static void display_all(void)
{
	printf("\033[J");

	for (i8 y = 1; y < ARRAY_SIZE(g.board) - 1; ++y)
		for (i8 x = 1; x < ARRAY_SIZE(*g.board) - 1; ++x)
			display_tile((Coords) {x, y});

	for (Trap *t = g.traps; t->pos.x; ++t)
		display_trap(t);

	cursor_to(64, 0);
	printf("\033[s");
	for (Monster *m = &player + 1; m->class; ++m)
		display_enemy(m);

	display_inventory();
	cursor_to(0, 0);
}

// `play` entry point: alternatively updates the interface and prompts the user for a command.
int main(i32 argc, char **argv)
{
	xml_parse(argc, argv);

	g.seed = (u64) time(NULL);

	system("stty -echo -icanon eol \1");
	printf("\033[?1049h");

	while (player.hp > 0 && !player_won()) {
		display_all();
		int c = getchar();
		if (c == 't')
			execv(*argv, argv);
		if (c == EOF || c == 'q')
			break;
		do_beat((u8) c);
	}

	printf("\033[?1049l");
	printf("%s!\n", player_won() ? "You won" : "See you soon");
}
