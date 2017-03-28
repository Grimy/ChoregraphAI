// play.c - manages terminal input/output
// Assumes an ANSI-compatible UTF-8 terminal with a black background.

#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include "chore.h"

// Terminal ANSI codes
#define CLEAR   "\033[m"
#define BOLD    "\033[1m"
#define REVERSE "\033[7m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define BROWN   "\033[33m"
#define BLUE    "\033[34m"
// #define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define GRAY    "\033[37m"
#define BLACK   "\033[90m"
#define ORANGE  "\033[91m"
#define SPRING  "\033[92m"
#define YELLOW  "\033[93m"
#define PURPLE  "\033[94m"
#define PINK    "\033[95m"
#define AZURE   "\033[96m"
#define WHITE   "\033[97m"

static Coords cursor;
static bool run_animations = false;

static const char* tile_glyphs[] = {
	[FLOOR] = ".",
	[WATER] = BLUE "≈",
	[TAR] = BLACK "≈",
	[STAIRS] = WHITE ">",
	[FIRE] = RED "░",
	[ICE] = CYAN "█",
	[OOZE] = GREEN "░",
	[EDGE] = " ",
	[DOOR] = BROWN "+",
	[DIRT] = WHITE, [DIRT | 8] = WHITE,
	[STONE] = WHITE, [STONE | 8] = WHITE,
	[STONE | FIRE] = RED,
	[STONE | ICE] = CYAN,
	[CATACOMB] = BLACK,
	[SHOP] = YELLOW,
};

static const char* trap_glyphs[] = {
	[OMNIBOUNCE] = BROWN "■",
	[SPIKE] = "◭",
	[TRAPDOOR] = BROWN "▫",
	[CONFUSE] = YELLOW "◆",
	[TELEPORT] = YELLOW "▫",
	[TEMPO_DOWN] = YELLOW "«",
	[TEMPO_UP] = YELLOW "»",
	[BOMBTRAP] = BROWN "●",
	[SCATTER_TRAP] = BROWN "×",
};

static const char* monster_glyphs[] = {
#define X(name, glyph, ai, ...) glyph,
#include "monsters.h"
};

static const char* item_glyphs[] = {
#define X(name, slot, friendly, glyph, power) glyph,
#include "items.h"
};

static const char* item_names[] = {
#define X(name, slot, friendly, glyph, power) friendly,
#include "items.h"
};

static const char* animation_steps[][6] = {
	[EXPLOSION]    = { WHITE "█", YELLOW "▓", ORANGE "▒", RED "▒", BLACK "░", "" },
	[FIREBALL]     = { WHITE "█", YELLOW "▇", ORANGE "▅", RED "▬", BROWN "▬", "" },
	[CONE_OF_COLD] = { WHITE "█", BLUE   "▓", BLUE   "▒", CYAN "▒", WHITE "░", "" },
	[SPORES]       = { CYAN "∵", CYAN "∴", "" },
	[ELECTRICITY]  = { YELLOW "↯", "↯", "\033[103m" REVERSE "↯", CLEAR "↯", "↯", "" },
	[BOUNCE_TRAP]  = { " ", "" },
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
	i64 glyph = 0;
	for (i64 i = 0; i < 4; ++i) {
		Tile &tile = TILE(pos + plus_shape[i]);
		glyph |= (tile.revealed && tile.type >= EDGE) << i;
	}
	print_at(pos, "%3.3s", &"──│┘│┐│┤──└┴┌┬├┼"[3 * glyph]);
}

static void display_wire(Coords pos)
{
	i64 glyph = 0;
	for (i64 i = 0; i < 4; ++i) {
		Tile &tile = TILE(pos + plus_shape[i]);
		glyph |= (tile.revealed && tile.wired) << i;
	}
	print_at(pos, YELLOW "%3.3s", &"⋅╴╵╯╷╮│┤╶─╰┴╭┬├┼"[3 * glyph]);
}

// Pretty-prints the tile at the given position.
static void display_tile(Coords pos)
{
	Tile *tile = &TILE(pos);

	int light = shadowed(pos) ? 0 :
		L2(pos - player.pos) <= player.radius ? 7777 :
		min(tile->light, 7777);
	printf("\033[38;5;%dm", 232 + light / 338);
	print_at(pos, tile_glyphs[tile->type]);

	if (IS_DIGGABLE(pos) && !IS_DOOR(pos))
		display_wall(pos);
	if (IS_WIRE(pos) && !IS_DOOR(pos))
		display_wire(pos);
	if (tile->item)
		print_at(pos, item_glyphs[tile->item]);
	if (!tile->revealed)
		print_at(pos, " ");
}

static void display_trap(const Trap *t)
{
	const char *glyph = t->type == BOUNCE ? dir_to_arrow(t->dir) : trap_glyphs[t->type];
	print_at(t->pos, WHITE "%s", glyph);
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
		return m->state ? RED REVERSE "breathing" : "";
	case EVIL_EYE_1:
	case EVIL_EYE_2:
		return m->state ? "glowing" : "";
	case DEVIL_1:
	case DEVIL_2:
		return m->state ? "unshelled" : "";
	case TARMONSTER:
	case MIMIC_1:
	case MIMIC_2:
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
		return m->state >= 2 ? RED REVERSE "breathing" : m->exhausted ? "exhausted" : "";
	case OGRE:
		return m->state == 2 ? "clonking" : "";
	}
	return "";
}

static void display_monster(const Monster *m, Coords &pos)
{
	++pos.y;
	if (cursor == m->pos || (cursor.x >= pos.x && cursor.y == pos.y))
		printf(REVERSE);
	print_at(m->pos, WHITE "%s", monster_glyphs[m->type]);
	if (m->type == SHOPKEEPER && g.current_beat % 2)
		printf("♪");
	print_at(pos, "%s ", monster_glyphs[m->type]);
	printf("%s", m->aggro ? ORANGE "!" : " ");
	printf("%s", m->delay ? BLACK "◔" : " ");
	printf("%s", m->confusion ? YELLOW "?" : " ");
	printf("%s", m->freeze ? CYAN "=" : " ");
	printf(WHITE "%s ", dir_to_arrow(m->dir));
	display_hearts(m);
	printf("%s\033[K" CLEAR, additional_info(m));
}

static void display_player(void)
{
	i8 x = 32, y = 0;
	print_at({x, ++y}, CLEAR "Bard ");
	display_hearts(&player);
	print_at({x, ++y}, "%32s" REVERSE, "");
	print_at({x, y}, "");
	printf("%s", g.monkeyed ? PURPLE "Monkeyed " : "");
	printf("%s", player.confusion ? YELLOW "Confused " : "");
	printf("%s", player.freeze ? CYAN "Frozen " : "");
	printf("%s", g.sliding_on_ice ? CYAN "Sliding" : "");
	printf("%s", g.iframes > g.current_beat ? PINK "I-framed " : "");
	print_at({x, ++y}, CLEAR);
	print_at({x, ++y}, "Bombs:  %-2d",  g.bombs);
	print_at({x, ++y}, "Shovel: %-20s", item_names[g.shovel]);
	print_at({x, ++y}, "Weapon: %-20s", item_names[g.weapon]);
	print_at({x, ++y}, "Body:   %-20s", item_names[g.body]);
	print_at({x, ++y}, "Head:   %-20s", item_names[g.head]);
	print_at({x, ++y}, "Boots:  %s%-20s" CLEAR, g.boots_on ? BOLD : "", item_names[g.feet]);
	print_at({x, ++y}, "Ring:   %-20s", item_names[g.ring]);
	print_at({x, ++y}, "Usable: %-20s", item_names[g.usable]);
	if (!player.untrapped)
		print_at(player.pos, "%s", tile_glyphs[TILE(player.pos).type]);
	print_at(player.pos, REVERSE "@" CLEAR);
}

// Clears and redraws the entire interface.
static void display_all(void)
{
	for (i8 y = 1; y < ARRAY_SIZE(g.board) - 1; ++y)
		for (i8 x = 1; x < ARRAY_SIZE(*g.board) - 1; ++x)
			display_tile({x, y});

	for (Trap *t = g.traps; t->pos.x; ++t)
		if (TILE(t->pos).revealed && !TILE(t->pos).destroyed)
			display_trap(t);

	Coords pos = {64, 1};
	for (Monster *m = &g.monsters[g.last_monster]; m != &player; --m)
		if (m->hp && (m->aggro || TILE(m->pos).revealed || g.head == HEAD_CIRCLET))
			display_monster(m, pos);
	for (++pos.y; pos.y < 50; ++pos.y)
		print_at(pos, "\033[K");

	display_player();
	print_at({0, 0}, "%s\033[K", g.game_over? player.hp ? "You won!" : "You died..." : "");
}

void animation(Animation id, Coords pos, Coords dir)
{
	if (!run_animations)
		return;
	static Coords targets[32];
	i64 target_count = 0;

	display_all();

	switch (id) {
	case EXPLOSION:
		for (Coords d: square_shape)
			targets[target_count++] = pos + d;
		break;
	case FIREBALL:
		for (Coords p = pos + dir; !BLOCKS_LOS(p); p += dir)
			targets[target_count++] = p;
		break;
	case CONE_OF_COLD:
		for (Coords d: cone_shape)
			targets[target_count++] = pos + d * dir.x;
		break;
	case SPORES:
		for (Coords d: square_shape)
			if (L1(d))
				targets[target_count++] = pos + d;
		break;
	case ELECTRICITY:
		for (Monster *m = &player; m->type; ++m) {
			targets[target_count] = m->pos;
			target_count += m->electrified;
			m->electrified = false;
		}
		break;
	case BOUNCE_TRAP:
		break;
	}

	for (const char **step = animation_steps[(i64) id]; **step; ++step) {
		for (i64 i = 0; i < target_count; ++i)
			if (TILE(pos).revealed && TILE(targets[i]).revealed)
				print_at(targets[i], *step);
		fflush(stdout);
		struct timeval timeout = { .tv_usec = 42000 };
		fd_set in_fds;
		FD_SET(0, &in_fds);
		select(1, &in_fds, NULL, NULL, &timeout);
	}
}

// `play` entry point: an interactive interface to the simulation.
int main(i32 argc, char **argv)
{
	g.seed = (u32) time(NULL);
	xml_parse(argc, argv);
	run_animations = true;

	system("stty -echo -icanon eol \1");
	printf("\033[?25l\033[?1003;1049h");
	atexit([]() { printf("\033[?25h\033[?1003;1049l"); });

	GameState timeline[32] = {[0 ... 31] = g};

	for (;;) {
		timeline[g.current_beat & 31] = g;
		display_all();
		i32 c = getchar();
		if (c == 't')
			execv(*argv, argv);
		else if (c == 'u')
			g = timeline[(g.current_beat - 1) & 31];
		else if (c == '\033' && scanf("[M%*c%c%c", &cursor.x, &cursor.y))
			cursor += {-33, -33};
		else if (c == EOF || c == 4 || c == 'q')
			break;
		else if (!g.game_over)
			g.game_over = do_beat((char) c);
	}
}
