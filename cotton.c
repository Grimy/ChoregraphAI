#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LENGTH(array) ((int) (sizeof(array) / sizeof(*(array))))
#define SIGN(x) (((x) > 0) - ((x) < 0))
#define ABS(x)  ((x) < 0 ? -(x) : (x))
#define CLASS(e) (class_infos[(e)->class])
#define SPAWN_Y 9
#define SPAWN_X 23
#define player entities

enum {
	SKELETON = 3,
	BLUE_BAT = 6,
	MONKEY = 9,

	BOMBER = 44,
	DIGGER,
	BLACK_BAT,
	ARMADILDO,
	BLADENOVICE,
	BLADEMASTER,
	GHOUL,
	OOZE_GOLEM,
	HARPY,
	LICH_1,
	LICH_2,
	LICH_3,
	CONF_MONKEY,
	TELE_MONKEY,
	PIXIE,
	SARCO_1,
	SARCO_2,
	SARCO_3,
	SPIDER,
	WARLOCK_1,
	WARLOCK_2,
	MUMMY,
	GARGOYLE_1,
	GARGOYLE_2,
	GARGOYLE_3,
	GARGOYLE_4,
	GARGOYLE_5,
	GARGOYLE_6,

	SHOPKEEPER = 88,
	BLUE_DRAGON = 148,
	MOMMY = 155,

	PLAYER,
	TRAP,
	DIRT,
	STONE,
};

typedef struct entity {
	struct entity *next;
	uint8_t class;
	uint8_t x;
	uint8_t y;
	uint8_t prev_x;
	uint8_t prev_y;
	int8_t hp;
	unsigned delay: 4;
	int dx: 2;
	int dy: 2;
	unsigned aggro: 1;
	int : 7;
} Entity;

typedef struct class {
	int8_t max_hp;
	uint8_t beat_delay;
	int16_t glyph;
	uint32_t priority;
	void (*act) (struct entity*);
} Class;

static Class class_infos[256];

static Entity dirt_wall = { .class = DIRT };
static Entity stone_wall = { .class = STONE };
__extension__ static Entity *board[32][32] = { [0 ... 31] = { [0 ... 31] = &stone_wall } };
static Entity entities[256];

static int dy, dx;

static void rm_ent(Entity *e) {
	Entity **prev;
	for (prev = &board[e->y][e->x]; *prev != e; prev = &(*prev)->next);
	*prev = e->next;
}

static void add_ent(Entity *e) {
	e->next = board[e->y][e->x];
	board[e->y][e->x] = e;
}

static void spawn(uint8_t class, uint8_t y, uint8_t x) {
	Entity *e;
	for (e = entities; e->class; ++e);
	*e = (Entity) {.class = (uint8_t) class, .y = y, .x = x, .hp = class_infos[class].max_hp};
}

static int has_wall(int y, int x) {
	if (y >= LENGTH(board) || x >= LENGTH(*board))
		return 0;
	for (Entity *e = board[y][x]; e; e = e->next)
		if (e->class == DIRT)
			return 1;
	return 0;
}

#include "los.c"
#include "ui.c"
#include "monsters.c"
#include "xml.c"

static int compare_priorities(const void *a, const void *b) {
	uint32_t pa = CLASS((const Entity*) a).priority;
	uint32_t pb = CLASS((const Entity*) b).priority;
	return (pb > pa) - (pb < pa);
}

int main(void) {
	system("stty -echo -icanon eol \1");
	parse_xml();
	spawn(PLAYER, SPAWN_Y, SPAWN_X);
	qsort(entities, LENGTH(entities), sizeof(*entities), compare_priorities);
	for (Entity *e = entities; CLASS(e).priority; ++e)
		add_ent(e);
	for (Entity *e = entities; player->hp; e = CLASS(e + 1).priority ? e + 1 : entities) {
		if (e->hp <= 0)
			continue;
		dy = player->y - e->y;
		dx = player->x - e->x;
		e->aggro = e->aggro || can_see(e->y, e->x);
		if (!e->aggro && dy * dy + dx * dx > 9)
			continue;
		if (e->delay) {
			e->delay--;
			continue;
		}
		CLASS(e).act(e);
	}
}
