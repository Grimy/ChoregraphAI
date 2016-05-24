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
	TRAP,
	DIRT,
	STONE,
};

typedef struct entity {
	struct entity *next;
	uint8_t class;
	int8_t x;
	int8_t y;
	int8_t prev_x;
	int8_t prev_y;
	int8_t hp;
	unsigned delay: 4;
	unsigned aggro: 1;
	unsigned vertical: 1;
	unsigned: 10;
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

// Creates a new entity and adds it to the list
static void spawn(uint8_t class, int8_t y, int8_t x) {
	static int entity_count = 0;
	Entity e = {.class = class, .y = y, .x = x, .hp = class_infos[class].max_hp};
	entities[entity_count++] = e;
}

// Remove an entity from the board
static void ent_rm(Entity *e) {
	Entity **prev;
	for (prev = &board[e->y][e->x]; *prev != e; prev = &(*prev)->next);
	*prev = e->next;
}

// Add an entity to the board
static void ent_add(Entity *e) {
	e->next = board[e->y][e->x];
	board[e->y][e->x] = e;
}

static void ent_move(Entity *e, int8_t y, int8_t x) {
	ent_rm(e);
	e->prev_y = e->y;
	e->prev_x = e->x;
	e->y = y;
	e->x = x;
	ent_add(e);
}

static int has_wall(int y, int x) {
	if (y >= LENGTH(board) || x >= LENGTH(*board))
		return 0;
	for (Entity *e = board[y][x]; e; e = e->next)
		if (e->class == DIRT)
			return 1;
	return 0;
}

static void knockback(Entity *e) {
	e->delay = 1;
}

static int can_move(Entity *e, int dy, int dx) {
	Entity *dest = board[e->y + dy][e->x + dx];
	return dest == NULL || dest->class == PLAYER;
}

static void monster_attack(Entity *attacker) {
	if (attacker->class == CONF_MONKEY) {
		attacker->hp = 0;
		ent_rm(attacker);
	} else {
		player->hp = 0;
	}
}

static int monster_move(Entity *e, int8_t y, int8_t x) {
	if (!(can_move(e, y, x)))
		return 0;
	e->delay = CLASS(e).beat_delay;
	Entity *dest = board[e->y + y][e->x + x];
	if (dest && dest->class == PLAYER)
		monster_attack(e);
	else
		ent_move(e, e->y + y, e->x + x);
	return 1;
}

static void player_attack(Entity *e) {
	e->hp -= 1;
	if (e->hp <= 0) {
		ent_rm(e);
		if (e->class == WARLOCK_1 || e->class == WARLOCK_2)
			ent_move(player, e->y, e->x);
	} else if (CLASS(e).beat_delay == 0) {
		knockback(e);
	}
}

static void player_move(Entity *this, int8_t y, int8_t x) {
	Entity *dest = board[this->y + y][this->x + x];
	if (dest == NULL)
		ent_move(this, this->y + y, this->x + x);
	else if (dest->class < PLAYER)
		player_attack(dest);
	else if (dest->class == DIRT)
		board[this->y + y][this->x + x] = NULL;
}

#include "los.c"
#include "ui.c"
#include "monsters.c"
#include "xml.c"

static void monster_turn(Entity *e) {
	dy = player->y - e->y;
	dx = player->x - e->x;
	e->aggro = e->aggro || can_see(e->y, e->x);
	if (!e->aggro && dy * dy + dx * dx > 9)
		return;
	if (e->delay) {
		e->delay--;
		return;
	}
	CLASS(e).act(e);
}

static void do_beat(void) {
	player_turn(player);
	for (Entity *e = entities + 1; CLASS(e).priority; ++e)
		if (e->hp > 0)
			monster_turn(e);
}

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
		ent_add(e);
	while (player->hp)
		do_beat();
}
