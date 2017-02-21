// play.c - manages terminal input/output
// Assumes an ANSI-compatible UTF-8 terminal with a black background.

#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include "chore.h"

static Coords cursor;
static bool run_animations = false;

static const char* floor_glyphs[] = {
	[FLOOR] = WHITE ".",
	[SHOP_FLOOR] = YELLOW ".",
	[WATER] = BLUE "≈",
	[TAR] = BLACK "≈",
	[STAIRS] = ">",
	[FIRE] = RED "░",
	[ICE] = CYAN "█",
	[OOZE] = GREEN "░",
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
	[SCATTER_TRAP] = BROWN "×",
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
	const char* color =
		TILE(pos).type & FIRE ? RED :
		TILE(pos).type & ICE ? CYAN :
		TILE(pos).type == CATACOMB ? BLACK :
		TILE(pos).type == SHOP ? YELLOW : WHITE;

	i64 glyph = 0;
	for (i64 i = 0; i < 4; ++i)
		glyph |= IS_DIGGABLE(pos + plus_shape[i]) << i;
	print_at(pos, "%s%3.3s", color, &"──│┘│┐│┤──└┴┌┬├┼"[3 * glyph]);
}

static void display_wire(Coords pos)
{
	i64 glyph = 0;
	for (i64 i = 0; i < 4; ++i)
		glyph |= (TILE(pos + plus_shape[i]).wired) << i;
	print_at(pos, YELLOW "%3.3s", &"⋅╴╵╯╷╮│┤╶─╰┴╭┬├┼"[3 * glyph]);
}

// Pretty-prints the tile at the given position.
static void display_tile(Coords pos)
{
	Tile *tile = &TILE(pos);

	if (tile->type == EDGE || !tile->revealed)
		print_at(pos, " ");
	else if (IS_DOOR(pos))
		print_at(pos, BROWN "+");
	else if (IS_DIGGABLE(pos))
		display_wall(pos);
	else if (IS_WIRE(pos))
		display_wire(pos);
	else if (tile->item)
		print_at(pos, item_names[tile->item].glyph);
	else if (!(g.nightmare && L2(pos - g.monsters[g.nightmare].pos) < 8))
		print_at(pos, floor_glyphs[tile->type]);
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
	print_at(m->pos, WHITE "%s", TYPE(m).glyph);
	if (m->type == SHOPKEEPER && g.current_beat % 2)
		printf("♪");
	print_at(pos, "%s ", TYPE(m).glyph);
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
	print_at({x, ++y}, "Bombs: %d", g.bombs);
	print_at({x, ++y}, "Shovel: %s", item_names[g.shovel].friendly);
	print_at({x, ++y}, "Weapon: %s", item_names[g.weapon].friendly);
	print_at({x, ++y}, "Body: %s",   item_names[g.body].friendly);
	print_at({x, ++y}, "Head: %s",   item_names[g.head].friendly);
	print_at({x, ++y}, "Boots: %s",  item_names[g.feet].friendly);
	printf(" (%s)", g.boots_on ? "on" : "off");
	print_at({x, ++y}, "Ring: %s",   item_names[g.ring].friendly);
	print_at({x, ++y}, "Usable: %s", item_names[g.usable].friendly);
	display_tile(player.pos);
	printf(REVERSE "\b@" CLEAR);
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
	print_at({0, 0}, g.player_won ? "You won!" : player.hp ? "" : "You died...");
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
			if (m->electrified) {
				targets[target_count++] = m->pos;
				m->electrified = false;
			}
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
	xml_parse(argc, argv);
	g.seed = (u32) time(NULL);
	if (argc == 4)
		while (*argv[3])
			do_beat((u8) *argv[3]++);

	run_animations = true;

	system("stty -echo -icanon eol \1");
	printf("\033[?25l\033[?1003h\033[?1049h");

	GameState timeline[32] = {[0 ... 31] = g};

	for (;;) {
		timeline[g.current_beat & 31] = g;
		display_all();
		int c = getchar();
		if (c == 't')
			execv(*argv, argv);
		else if (c == 4 || c == 'q')
			break;
		else if (c == '\033' && scanf("[M%*c%c%c", &cursor.x, &cursor.y))
			cursor += {-33, -33};
		else if (c == 'u')
			g = timeline[(g.current_beat - 1) & 31];
		else if (player.hp && !g.player_won)
			do_beat((u8) c);
		else
			break;
	}

	printf("\033[?25h\033[?1003l\033[?1049l");
}
