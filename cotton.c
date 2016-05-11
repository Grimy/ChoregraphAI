#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LENGTH(array) ((int) (sizeof(array) / sizeof(*(array))))
#define SIGN(x) (((x) > 0) - ((x) < 0))
#define ABS(x)  ((x) < 0 ? -(x) : (x))
#define CLASS(e) (class_infos[(e)->class])
#define SPAWN_Y 9
#define SPAWN_X 26

enum {
	PLAYER = 1,
	SKELETON = 4,
	BLACK_BAT = 6,
	SPIDER = 62,
	SHOPKEEPER = 88,
	BLUE_DRAGON = 148,
	TRAP,
	DIRT,
	STONE,
};

typedef struct entity {
	struct entity *next;
	uint8_t class;
	uint8_t x;
	uint8_t y;
	int8_t hp;
	int8_t dx;
	int8_t dy;
	uint16_t state;
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

static int prev_y;
static int prev_x;

static void rm_ent(Entity *e) {
	Entity **prev;
	for (prev = &board[e->y][e->x]; *prev != e; prev = &(*prev)->next);
	*prev = e->next;
}

static void add_ent(Entity *e) {
	e->next = board[e->y][e->x];
	board[e->y][e->x] = e;
}

static int has_wall(int y, int x) {
	for (Entity *e = board[y][x]; e; e = e->next)
		if (e->class == DIRT)
			return 1;
	return 0;
}

static void move_ent(Entity *e) {
	rm_ent(e);
	e->y += e->dy;
	e->x += e->dx;
	add_ent(e);
}

static int can_move(Entity *e, int dy, int dx) {
	return board[e->y + dy][e->x + dx] == NULL;
}

static void spawn(uint8_t class, uint8_t y, uint8_t x) {
	Entity *e;
	for (e = entities; e->class; ++e);
	*e = (Entity) {.class = (uint8_t) class, .y = y, .x = x, .hp = class_infos[class].max_hp};
}

#include "ui.c"
#include "monsters.c"
#include "xml.c"

static int compare_priorities(const void *a, const void *b) {
	uint32_t pa = CLASS((Entity*) a).priority;
	uint32_t pb = CLASS((Entity*) b).priority;
	return (pb > pa) - (pb < pa);
}

int main(void) {
	system("stty -echo -icanon eol \1");
	parse_xml();
	spawn(PLAYER, SPAWN_Y, SPAWN_X);
	qsort(entities, LENGTH(entities), sizeof(*entities), compare_priorities);
	for (Entity *e = entities; CLASS(e).priority; ++e)
		add_ent(e);
	for (Entity *e = entities; entities->hp; e = CLASS(e + 1).priority ? e + 1 : entities)
		CLASS(e).act(e);
}
