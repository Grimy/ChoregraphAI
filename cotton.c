#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LENGTH(array) ((long) (sizeof(array) / sizeof(*(array))))
#define SIGN(x) (((x) > 0) - ((x) < 0))
#define ABS(x)  ((x) < 0 ? -(x) : (x))

#define IS_MONSTER(m) ((m) && (m)->class != PLAYER)
#define CLASS(m) (class_infos[(m)->class])
#define SPAWN_Y 9
#define SPAWN_X 24

typedef unsigned long bool;

typedef enum __attribute__((__packed__)) {
	SKELETON = 3,
	BLUE_BAT = 6,
	MONKEY = 9,

	BOMBER = 44,
	DIGGER,
	BLACK_BAT,
	ARMADILDO,
	BLADENOVICE, BLADEMASTER,
	GHOUL,
	OOZE_GOLEM,
	HARPY,
	LICH_1, LICH_2, LICH_3,
	CONF_MONKEY,
	TELE_MONKEY,
	PIXIE,
	SARCO_1, SARCO_2, SARCO_3,
	SPIDER,
	WARLOCK_1, WARLOCK_2,
	MUMMY,
	GARGOYLE_1, GARGOYLE_2, GARGOYLE_3, GARGOYLE_4, GARGOYLE_5, GARGOYLE_6,

	SHOPKEEPER = 88,
	BLUE_DRAGON = 148,
	MOMMY = 155,

	PLAYER,
} MonsterClass;

typedef enum __attribute__((__packed__)) {
	WALL,
	FLOOR,
	WATER,
	TAR,
	FIRE,
	ICE,
	OOZE,
	STAIRS,
} TileClass;

typedef enum __attribute__((__packed__)) {
	TRAP,
} TrapClass;

typedef struct {
	MonsterClass class;
	int8_t x;
	int8_t y;
	int8_t prev_x;
	int8_t prev_y;
	int8_t hp;
	unsigned delay: 4;
	unsigned aggro: 1;
	unsigned vertical: 1;
	unsigned state: 2;
	unsigned: 8;
} Monster;

typedef struct {
	TileClass class;
	int8_t hp;
	int8_t torch;
	int8_t zone;
	int8_t revealed;
	unsigned: 24;
	Monster *next;
} Tile;

typedef struct {
	TrapClass class;
	int8_t x;
	int8_t y;
} Trap;

typedef struct {
	int8_t max_hp;
	uint8_t beat_delay;
	unsigned: 16;
	uint32_t priority;
	char *glyph;
	void (*act) (Monster*, long, long);
} ClassInfos;

static ClassInfos class_infos[256];

__extension__
static Tile board[32][32] = {[0 ... 31] = {[0 ... 31] = {.class = WALL, .hp = 5}}};
static Monster player = {.class = PLAYER, .hp = 1, .y = SPAWN_Y, .x = SPAWN_X};
static Monster monsters[256];
static Trap traps[256];

static void ent_move(Monster *m, int8_t y, int8_t x) {
	board[m->y][m->x].next = NULL;
	m->prev_y = m->y;
	m->prev_x = m->x;
	m->y = y;
	m->x = x;
	board[m->y][m->x].next = m;
}

static bool can_move(Monster *m, long dy, long dx) {
	Tile dest = board[m->y + dy][m->x + dx];
	return dest.class != WALL && !IS_MONSTER(dest.next);
}

static void monster_attack(Monster *attacker) {
	if (attacker->class == CONF_MONKEY || attacker->class == PIXIE) {
		attacker->hp = 0;
		board[attacker->y][attacker->x].next = NULL;
	} else {
		player.hp = 0;
	}
}

static bool monster_move(Monster *m, int8_t y, int8_t x) {
	if (!(can_move(m, y, x)))
		return 0;
	m->delay = CLASS(m).beat_delay;
	Tile dest = board[m->y + y][m->x + x];
	if (dest.next == &player)
		monster_attack(m);
	else
		ent_move(m, m->y + y, m->x + x);
	return 1;
}

static void knockback(Monster *m) {
	monster_move(m, SIGN(m->y - player.y), SIGN(m->x - player.x));
	m->delay = 1;
}

static void player_attack(Monster *m) {
	if (board[player.y][player.x].class == OOZE)
		return;
	if (m->class == OOZE_GOLEM)
		board[player.y][player.x].class = OOZE;
	if ((m->class == BLADENOVICE || m->class == BLADEMASTER) && m->state < 2) {
		knockback(m);
		m->state = 1;
		return;
	}
	m->hp -= 1;
	if (m->hp <= 0) {
		board[m->y][m->x].next = NULL;
		if (m->class == WARLOCK_1 || m->class == WARLOCK_2)
			ent_move(&player, m->y, m->x);
	} else if (CLASS(m).beat_delay == 0) {
		knockback(m);
	}
}

static void zone4_dig(Tile *tile) {
	if (tile->hp == 1 || tile->hp == 2)
		tile->class = FLOOR;
}

static void player_dig(Tile *wall) {
	long dig = board[player.y][player.x].class == OOZE ? 0 : 2;
	if (dig < wall->hp)
		return;
	wall->class = FLOOR;
	if (wall->zone == 4 && (wall->hp == 1 || wall->hp == 2)) {
		zone4_dig(wall - 1);
		zone4_dig(wall + 1);
		zone4_dig(wall - LENGTH(*board));
		zone4_dig(wall + LENGTH(*board));
	}
}

static void player_move(int8_t y, int8_t x) {
	Tile *dest = &board[player.y + y][player.x + x];
	player.prev_y = player.y;
	player.prev_x = player.x;
	if (dest->class == WALL)
		player_dig(dest);
	else if (IS_MONSTER(dest->next))
		player_attack(dest->next);
	else
		ent_move(&player, player.y + y, player.x + x);
}

#include "los.c"
#include "ui.c"
#include "monsters.c"
#include "xml.c"

static void trap_turn(Trap *this) {
	Monster *target = board[this->y][this->x].next;
	if (target == NULL)
		return;
	target->hp = 0;
	board[target->y][target->x].next = NULL;
}

static void monster_turn(Monster *m) {
	long dy = player.y - m->y;
	long dx = player.x - m->x;
	m->aggro = m->aggro || can_see(m->y, m->x);
	if (!m->aggro && dy * dy + dx * dx > 9)
		return;
	if (m->delay) {
		m->delay--;
		return;
	}
	CLASS(m).act(m, dy, dx);
}

static void do_beat(void) {
	player_turn();
	for (Monster *m = monsters; m->y; ++m)
		if (m->hp > 0)
			monster_turn(m);
	for (Trap *t = traps; t->y; ++t)
		trap_turn(t);
}

static int compare_priorities(const void *a, const void *b) {
	uint32_t pa = CLASS((const Monster*) a).priority;
	uint32_t pb = CLASS((const Monster*) b).priority;
	return (pb > pa) - (pb < pa);
}

int main(int argc, char **argv) {
	if (argc != 2)
		exit(argc);
	xml_parse(argv[1]);
	qsort(monsters, LENGTH(monsters), sizeof(*monsters), compare_priorities);
	for (Monster *m = monsters; CLASS(m).priority; ++m) {
		m->hp = CLASS(m).max_hp;
		board[m->y][m->x].next = m;
	}
	system("stty -echo -icanon eol \1");
	while (player.hp)
		do_beat();
}
