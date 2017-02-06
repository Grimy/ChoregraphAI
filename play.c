// play.c - manages terminal input/output
// Assumes an ANSI-compatible UTF-8 terminal with a black background.
// ♪

#include <time.h>
#include <unistd.h>

#include "chore.h"

#define LINE(...) printf("\033[u\033[B\033[s " __VA_ARGS__)

static const char* floor_glyphs[] = {
	".", ".", ".",
	[SHOP_FLOOR] = YELLOW ".",
	[WATER] = BLUE ".",
	[TAR] = BLACK ".",
	[STAIRS] = ">",
	[FIRE] = RED ".",
	[ICE] = CYAN ".",
	[OOZE] = GREEN ".",
	[WIRE] = ".",
};

static const char* trap_glyphs[] = {
	[OMNIBOUNCE] = BROWN "■",
	[SPIKE] = "◭",
	[TRAPDOOR] = BROWN "▫",
	[CONFUSE] = YELLOW "◆",
	[TELEPORT] = YELLOW "▫",
	[TEMPO_DOWN] = YELLOW "⇐",
	[TEMPO_UP] = YELLOW "⇒",
	[BOMBTRAP] = BROWN "●",
};

static const char* dir_to_arrow(Coords dir)
{
	static const char *arrows[] = {
		"↖", "↑", "↗",
		"←", " ", "→",
		"↙", "↓", "↘",
	};
	return arrows[3 * (dir.y + 1) + (dir.x + 1)];
}

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

	if (tile->monster)
		printf("%s", CLASS(&MONSTER(pos)).glyph);
	else if (IS_DOOR(pos))
		putchar('+');
	else if (IS_DIGGABLE(pos))
		display_wall(pos);
	else if (IS_WIRE(pos))
		display_wire(pos);
	else if (tile->item)
		putchar('*');
	else if (L2(pos - g.monsters[g.nightmare].pos) >= 8)
		printf("%s", floor_glyphs[tile->class]);

	printf(WHITE);
}

static void display_hearts(Monster *m)
{
	printf("%c%02d (%c%02d)", 64 + m->pos.x, m->pos.y, 64 + m->prev_pos.x, m->prev_pos.y);
	printf(RED " %.*s%.*s" WHITE, 3 * m->hp, "ღღღღღღღღღ", 9 - m->hp, "         ");
}

static void display_inventory(void)
{
	cursor_to(32, 0);
	printf("\033[s");
	LINE("Bard ");
	display_hearts(&player);
	LINE("%s%s%s%s%s",
		g.monkeyed ? PURPLE "Monkeyed " : "",
		player.confusion ? YELLOW "Confused " : "",
		player.freeze ? CYAN "Frozen " : "",
		g.sliding_on_ice ? CYAN "Sliding " : "",
		g.iframes > g.current_beat ? PINK "I-framed " : "");
	LINE("Beat #%d", g.current_beat);
	LINE("");
	LINE("Bombs: %d", g.inventory[BOMBS]);
	LINE("Weapon: %s", g.inventory[JEWELED] ? "Jeweled Dagger" : "Dagger");
	if (g.inventory[LUNGING])
		LINE("Boots: Lunging (%s)", g.boots_on ? "on" : "off");
	if (g.inventory[MEMERS_CAP])
		LINE("Head: %s", "Miner’s Cap");
}

static const char* additional_info(Monster *m)
{
	switch (m->class) {
	case BARREL:
		return m->state ? "rolling" : "";
	case ARMADILLO_1:
	case ARMADILLO_2:
	case ARMADILDO:
	case MINOTAUR_1:
	case MINOTAUR_2:
		return m->state ? "charging" : "";
	case GHOST:
		return m->state ? "" : "phased out";
	case ASSASSIN_1:
	case ASSASSIN_2:
		return m->state ? "" : "fleeing";
	case MOLE:
		return m->state ? "" : "burrowed";
	case DIGGER:
		return m->state ? "" : "sleeping";
	case BLADENOVICE:
	case BLADEMASTER:
		return m->state == 1 ? "parrying" :
			m->state == 2 ? "vulnerable" : "";
	case BOMB_STATUE:
		return m->state ? "active" : "";
	case FIREPIG:
		return m->state ? "breathing" : "";
	case EVIL_EYE_1:
	case EVIL_EYE_2:
		return m->state ? "glowing" : "";
	case DEVIL_1:
	case DEVIL_2:
		return m->state ? "unshelled" : "";
	case TARMONSTER:
	case MIMIC_1:
	case MIMIC_2:
	case MIMIC_3:
	case MIMIC_4:
	case MIMIC_5:
	case WHITE_MIMIC:
	case WALL_MIMIC:
	case MIMIC_STATUE:
	case FIRE_MIMIC:
	case ICE_MIMIC:
	case SHOP_MIMIC:
		return m->state == 2 ? "vulnerable" :
			m->state ? "" : "hidden";
	case RED_DRAGON:
	case BLUE_DRAGON:
		return m->state >= 2 ? "breathing" : m->exhausted ? "exhausted" : "";
	case OGRE:
		return m->state == 2 ? "clonking" : "";
	}
	return "";
}

static void display_enemy(Monster *m)
{
	if (!m->hp)
		return;
	LINE("%s %s%s%s%s" WHITE "%s ",
		CLASS(m).glyph,
		m->aggro ? ORANGE "!" : " ",
		m->delay ? BLACK "◔" : " ",
		m->confusion ? YELLOW "?" : " ",
		m->freeze ? CYAN "=" : " ",
		dir_to_arrow(m->dir));
	display_hearts(m);
	printf("%s", additional_info(m));
}

static void display_trap(Trap *t)
{
	if (TILE(t->pos).monster)
		return;
	cursor_to(t->pos.x, t->pos.y);
	printf("%s" WHITE, t->class == BOUNCE ? dir_to_arrow(t->dir) : trap_glyphs[t->class]);
}

// Clears and redraws the entire interface.
static void display_all(void)
{
	printf("\033[J");

	for (i8 y = 1; y < ARRAY_SIZE(g.board) - 1; ++y)
		for (i8 x = 1; x < ARRAY_SIZE(*g.board) - 1; ++x)
			display_tile((Coords) {x, y});

	for (Trap *t = g.traps; t->pos.x; ++t)
		if (TILE(t->pos).revealed && !TILE(t->pos).destroyed)
			display_trap(t);

	cursor_to(64, 0);
	printf("\033[s");

	for (Monster *m = &player + 1; m->class; ++m)
		if (m->aggro || TILE(m->pos).revealed)
			display_enemy(m);

	display_inventory();
	cursor_to(0, 0);
}

// `play` entry point: alternatively updates the interface and prompts the user for a command.
int main(i32 argc, char **argv)
{
	xml_parse(argc, argv);
	g.seed = (u64) time(NULL);
	if (argc == 4)
		while (*argv[3])
			do_beat((u8) *argv[3]++);

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
