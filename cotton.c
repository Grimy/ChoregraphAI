#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LENGTH(array) ((int) (sizeof(array) / sizeof(*(array))))
#define SIGN(x) (((x) > 0) - ((x) < 0))
#define ABS(x)  ((x) < 0 ? -(x) : (x))

#define IS_MONSTER(e) ((e) && (e)->class < PLAYER)
#define CLASS(e) (class_infos[(e)->class])
#define SPAWN_Y 9
#define SPAWN_X 24
#define player entities

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
	TRAP,
	FLOOR,
	WATER,
	TAR,
	FIRE,
	ICE,
	OOZE,
	STAIRS,
	WALL,
} Class;

typedef struct entity {
	struct entity *next;
	Class class;
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
} Entity;

typedef struct {
	int8_t max_hp;
	uint8_t beat_delay;
	int16_t glyph;
	uint32_t priority;
	void (*act) (struct entity*);
} ClassInfos;

static ClassInfos class_infos[256];

__extension__
static Entity board[32][32] = {[0 ... 31] = {[0 ... 31] = {.class = WALL, .hp = 5}}};
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
	Entity *prev;
	for (prev = &board[e->y][e->x]; prev->next != e; prev = prev->next);
	prev->next = e->next;
}

// Add an entity to the board
static void ent_add(Entity *e) {
	e->next = board[e->y][e->x].next;
	board[e->y][e->x].next = e;
}

static void ent_move(Entity *e, int8_t y, int8_t x) {
	ent_rm(e);
	e->prev_y = e->y;
	e->prev_x = e->x;
	e->y = y;
	e->x = x;
	ent_add(e);
}

static int can_move(Entity *e, int dy, int dx) {
	Entity dest = board[e->y + dy][e->x + dx];
	return dest.class != WALL && !IS_MONSTER(dest.next);
}

static void monster_attack(Entity *attacker) {
	if (attacker->class == CONF_MONKEY || attacker->class == PIXIE) {
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
	Entity dest = board[e->y + y][e->x + x];
	if (dest.next == player)
		monster_attack(e);
	else
		ent_move(e, e->y + y, e->x + x);
	return 1;
}

static void knockback(Entity *e) {
	monster_move(e, SIGN(e->y - player->y), SIGN(e->x - player->x));
	e->delay = 1;
}

static void player_attack(Entity *e) {
	if (board[player->y][player->x].class == OOZE)
		return;
	if (e->class == OOZE_GOLEM)
		board[player->y][player->x].class = OOZE;
	if ((e->class == BLADENOVICE || e->class == BLADEMASTER) && e->state < 2) {
		knockback(e);
		e->state = 1;
		return;
	}
	e->hp -= 1;
	if (e->hp <= 0) {
		ent_rm(e);
		if (e->class == WARLOCK_1 || e->class == WARLOCK_2)
			ent_move(player, e->y, e->x);
	} else if (CLASS(e).beat_delay == 0) {
		knockback(e);
	}
}

static void zone4_dig(Entity *e) {
	if (e->hp == 1 || e->hp == 2)
		e->class = FLOOR;
}

static void player_dig(Entity *wall) {
	if (wall->class != WALL)
		return;
	int dig = board[player->y][player->x].class == OOZE ? 0 : 2;
	if (dig < wall->hp)
		return;
	wall->class = FLOOR;
	if (wall->hp == 1 || wall->hp == 2) {
		zone4_dig(wall - 1);
		zone4_dig(wall + 1);
		zone4_dig(wall - LENGTH(*board));
		zone4_dig(wall + LENGTH(*board));
	}
}

static void player_move(int8_t y, int8_t x) {
	Entity *dest = &board[player->y + y][player->x + x];
	if (dest->class == WALL)
		player_dig(dest);
	else if (IS_MONSTER(dest->next))
		player_attack(dest->next);
	else
		ent_move(player, player->y + y, player->x + x);
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
	player_turn();
	for (Entity *e = entities + 1; CLASS(e).priority; ++e)
		if (e->hp > 0)
			monster_turn(e);
}

static int compare_priorities(const void *a, const void *b) {
	uint32_t pa = CLASS((const Entity*) a).priority;
	uint32_t pb = CLASS((const Entity*) b).priority;
	return (pb > pa) - (pb < pa);
}

int main(int argc, char **argv) {
	if (argc != 2)
		exit(argc);
	xml_parse(argv[1]);
	qsort(entities, LENGTH(entities), sizeof(*entities), compare_priorities);
	for (Entity *e = entities; CLASS(e).priority; ++e)
		ent_add(e);
	system("stty -echo -icanon eol \1");
	while (player->hp)
		do_beat();
}
