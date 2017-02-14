// play.c - manages terminal input/output
// Assumes an ANSI-compatible UTF-8 terminal with a black background.

#include <time.h>
#include <unistd.h>

#include "chore.h"

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

static void print_at(Coords pos, const char *fmt, ...) {
	printf("\033[%d;%dH", pos.y + 1, pos.x + 1);
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
}

// Picks an appropriate box-drawing glyph for a wall by looking at adjacent tiles.
// For example, when tiles to the bottom and right are walls too, use '┌'.
static void display_wall(Coords pos)
{
	if (TILE(pos).type == FIREWALL)
		printf(RED);
	else if (TILE(pos).type == ICEWALL)
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

	print_at(pos, WHITE);

	if (tile->type == EDGE || !tile->revealed)
		putchar(' ');
	else if (IS_DOOR(pos))
		putchar('+');
	else if (IS_DIGGABLE(pos))
		display_wall(pos);
	else if (IS_WIRE(pos))
		display_wire(pos);
	else if (tile->item)
		printf("%s", item_names[tile->item].glyph);
	else if (L2(pos - g.monsters[g.nightmare].pos) >= 8)
		printf("%s", floor_glyphs[tile->type]);
}

static void display_trap(const Trap *t)
{
	if (TILE(t->pos).monster)
		return;
	print_at(t->pos, WHITE);
	printf("%s", t->type == BOUNCE ? dir_to_arrow(t->dir) : trap_glyphs[t->type]);
}

static void display_hearts(const Monster *m)
{
	printf("%c%02d ", 64 + m->pos.x, m->pos.y);
	printf("(%c%02d)", 64 + m->prev_pos.x, m->prev_pos.y);
	printf(RED " %.*s%.*s" WHITE, 3 * m->hp, "ღღღღღღღღღ", 9 - m->hp, "         ");
}

static const char* additional_info(const Monster *m)
{
	switch (m->type) {
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

static void display_monster(const Monster *m, Coords &pos)
{
	++pos.y;
	print_at(pos, "%s %s%s%s%s" WHITE "%s ",
		TYPE(m).glyph,
		m->aggro ? ORANGE "!" : " ",
		m->delay ? BLACK "◔" : " ",
		m->confusion ? YELLOW "?" : " ",
		m->freeze ? CYAN "=" : " ",
		dir_to_arrow(m->dir));
	display_hearts(m);
	printf("%s", additional_info(m));

	print_at(m->pos, "%s" WHITE, TYPE(m).glyph);
	if (g.current_beat % 2 && m->type == SHOPKEEPER)
		printf("♪");
}

static void display_player(void)
{
	i8 x = 32, y = 0;
	print_at({x, ++y}, "Bard ");
	display_hearts(&player);
	print_at({x, ++y}, "%s%s%s%s%s" WHITE,
		g.monkeyed ? PURPLE "Monkeyed " : "",
		player.confusion ? YELLOW "Confused " : "",
		player.freeze ? CYAN "Frozen " : "",
		g.sliding_on_ice ? CYAN "Sliding " : "",
		g.iframes > g.current_beat ? PINK "I-framed " : "");
	print_at({x, ++y}, "Beat #%d", g.current_beat);
	print_at({x, ++y}, "Bombs: %d", g.bombs);
	print_at({x, ++y}, "Shovel: %s", item_names[g.shovel].friendly);
	print_at({x, ++y}, "Weapon: %s", item_names[g.weapon].friendly);
	print_at({x, ++y}, "Body: %s",   item_names[g.body].friendly);
	print_at({x, ++y}, "Head: %s",   item_names[g.head].friendly);
	print_at({x, ++y}, "Boots: %s",  item_names[g.feet].friendly);
	printf(" (%s)", g.boots_on ? "on" : "off");
	print_at({x, ++y}, "Ring: %s",   item_names[g.ring].friendly);
	print_at({x, ++y}, "Usable: %s", item_names[g.usable].friendly);
	print_at(player.pos, "\033[7m@" WHITE);
}

// Clears and redraws the entire interface.
static void display_all(void)
{
	printf("\033[J");

	for (i8 y = 1; y < ARRAY_SIZE(g.board) - 1; ++y)
		for (i8 x = 1; x < ARRAY_SIZE(*g.board) - 1; ++x)
			display_tile({x, y});

	for (Trap *t = g.traps; t->pos.x; ++t)
		if (TILE(t->pos).revealed && !TILE(t->pos).destroyed)
			display_trap(t);

	Coords pos = {64, 0};
	for (Monster *m = &player + 1; m->type; ++m)
		if (m->hp && (m->aggro || TILE(m->pos).revealed || g.head == HEAD_CIRCLET))
			display_monster(m, pos);

	display_player();
	print_at({0, 0}, "");
}

// `play` entry point: an interactive interface to the simulation.
int main(i32 argc, char **argv)
{
	xml_parse(argc, argv);
	g.seed = (u32) time(NULL);
	if (argc == 4)
		while (*argv[3])
			do_beat((u8) *argv[3]++);

	system("stty -echo -icanon eol \1");
	printf("\033[?1049h");

	while (player.hp) {
		display_all();
		int c = getchar();
		if (c == 't')
			execv(*argv, argv);
		else if (c == EOF || c == 'q')
			player.hp = 0;
		else if (do_beat((u8) c))
			break;
	}

	printf("\033[?1049l");
	printf("%s!\n", player.hp ? "You won" : "See you soon");
}
