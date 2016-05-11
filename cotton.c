#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/xmlreader.h>

#define LENGTH(array) ((int) (sizeof(array) / sizeof(*(array))))
#define SIGN(x) (((x) > 0) - ((x) < 0))
#define ABS(x)  ((x) < 0 ? -(x) : (x))
#define CLASS(e) (class_infos[e->class])
#define SPAWN_Y 9
#define SPAWN_X 26

// At minima, we need:
// 4 bits for hp
// 2 bits for direction
// 4 bits for beat delay (10 for sarcophagi)
// 1 (2?) bit(s) for aggro/active status

typedef enum class {
	PLAYER,
	SKELETON = 4,
	WALL,
} Class;

typedef struct entity {
	struct entity *next;
	uint8_t class;
	uint8_t x;
	uint8_t y;
	uint8_t hp;
	int8_t dx;
	int8_t dy;
	uint16_t state;
} Entity;

static void player_input(struct entity*);
static void basic_seek(struct entity*);

static struct {
	uint8_t max_hp;
	uint8_t beat_delay;
	int16_t glyph;
	uint32_t priority;
	void (*act) (struct entity*);
} class_infos[256] = {
	[PLAYER]   = { 1, 0, '@', 9999, player_input },
	[SKELETON] = { 1, 1, 'Z', 1001, basic_seek },
	[6]        = { 1, 1, 'a',    1, basic_seek },
	[44]       = { 1, 1, 'A',    1, basic_seek },
	[45]       = { 1, 1, 'B',    1, basic_seek },
	[46]       = { 1, 1, 'C',    1, basic_seek },
	[48]       = { 1, 1, 'D',    1, basic_seek },
	[51]       = { 1, 1, 'E',    1, basic_seek },
	[52]       = { 1, 1, 'F',    1, basic_seek },
	[53]       = { 1, 1, 'G',    1, basic_seek },
	[56]       = { 1, 1, 'H',    1, basic_seek },
	[57]       = { 1, 1, 'I',    1, basic_seek },
	[59]       = { 1, 1, 'J',    1, basic_seek },
	[62]       = { 1, 1, 'K',    1, basic_seek },
	[63]       = { 1, 1, 'L',    1, basic_seek },
	[66]       = { 1, 1, 'M',    1, basic_seek },
	[67]       = { 1, 1, 'N',    1, basic_seek },
	[68]       = { 1, 1, 'O',    1, basic_seek },
	[88]       = { 1, 1, 'P',    1, basic_seek },
	[148]      = { 1, 1, 'O',    1, basic_seek },
	[WALL]     = { 0, 0, '+',    0, NULL },
};

static Entity *board[32][32];
static Entity entities[128];

static int prev_y;
static int prev_x;

static void display(void) {
	// printf("\033[H\033[2J");
	for (int y = 0; y < LENGTH(board); ++y) {
		for (int x = 0; x < LENGTH(*board); ++x) {
			Entity *e = board[y][x];
			putchar(e ? CLASS(e).glyph : '.');
		}
		putchar('\n');
	}
}

static void rm_ent(Entity *e) {
	Entity **prev;
	for (prev = &board[e->y][e->x]; *prev != e; prev = &(*prev)->next);
	*prev = e->next;
}

static void add_ent(Entity *e) {
	e->next = board[e->y][e->x];
	board[e->y][e->x] = e;
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

static void player_input(Entity *this) {
	display();
	switch (getchar()) {
		case 'e': this->dy =  0; this->dx = -1; break;
		case 'f': this->dy =  1; this->dx =  0; break;
		case 'i': this->dy =  0; this->dx =  1; break;
		case 'j': this->dy = -1; this->dx =  0; break;
		case 't': this->hp = 0; return;
		default: return;
	}
	if (can_move(this, this->dy, this->dx)) {
		prev_y = this->y;
		prev_x = this->x;
		move_ent(this);
	}
}

static void basic_seek(Entity *this) {
	if (this->state) {
		--this->state;
		return;
	}

	Entity *player = entities;
	int dy = player->y - this->y;
	int dx = player->x - this->x;
	int pdy = prev_y - this->y;
	int pdx = prev_x - this->x;

	int vertical =
		// #1: move towards the player
		dy == 0 ? 0 :
		dx == 0 ? 1 :

		// #2: avoid obstacles
		!can_move(this, SIGN(dy), 0) ? 0 :
		!can_move(this, 0, SIGN(dx)) ? 1 :
	
		// #3: move towards the player’s previous position
		pdy == 0 ? 0 :
		pdx == 0 ? 1 :

		// #4: keep moving in the same direction
		this->dx * dx > 0 ? 0 :
		this->dy * dy > 0 ? 1 :

		// #5: weird priority rules
		ABS(dy) == 1 && dx < 0 ? 0 :
		ABS(dx) == 1 ? 1 :
		ABS(dy) == 1 ? 0 :
		// dx == -2 && ABS(dy) == 2 ? 0 :
		// dx == 2 && ABS(dy) == 2 ? y > 0 :
		ABS(pdy) == 1 ? 0 :
		ABS(pdx) == 1 ? 1 :

		// #6: keep moving along the same axis
		!!this->dy;

	this->dy = vertical ? SIGN(dy) : 0;
	this->dx = vertical ? 0 : SIGN(dx);

	if (can_move(this, this->dy, this->dx)) {
		move_ent(this);
		this->state = CLASS(this).beat_delay;
	}
}

static void spawn(uint8_t class, uint8_t y, uint8_t x) {
	Entity *e;
	// uint32_t prio = class_infos[class].priority;
	// for (e = entities; e->hp && CLASS(e).priority >= prio; ++e);
	for (e = entities; e->hp; ++e);
	*e = (Entity) {.class = (uint8_t) class, .y = y, .x = x, .hp = class_infos[class].max_hp};
	add_ent(e);
}

static int xml_attr(xmlTextReaderPtr xml, const char* attr) {
	char* value = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) (attr));
	int result = atoi(value);
	free(value);
	return result;
}

static void process_node(xmlTextReaderPtr xml) {
	const char *name = (char*) xmlTextReaderConstName(xml);
	if (!strcmp(name, "tile") || !strcmp(name, "trap")) {
		// printf("%d %d %d\n", xml_attr(xml, "type"), xml_attr(xml, "y"), xml_attr(xml, "x"));
	} else if (!strcmp(name, "enemy")) {
		printf("%d %d %d\n", xml_attr(xml, "type"), xml_attr(xml, "y"), xml_attr(xml, "x"));
		spawn((uint8_t) xml_attr(xml, "type"), (uint8_t) xml_attr(xml, "y") + SPAWN_Y, (uint8_t) xml_attr(xml, "x") + SPAWN_X);
	}
}

static void parse_xml(void) {
	LIBXML_TEST_VERSION;
	xmlTextReaderPtr xml = xmlReaderForFile("LUNGEBARD.xml", NULL, 0);
	if (xml == NULL)
		exit(1);
	while (xmlTextReaderRead(xml) == 1) {
		if (xmlTextReaderNodeType(xml) != 1)
			continue;
		process_node(xml);
	}
	xmlFreeTextReader(xml);
}

int main(void) {
	spawn(PLAYER, SPAWN_Y, SPAWN_X);
	parse_xml();
	// if (!prev_x)
		// return 0;
	system("stty -echo -icanon eol \1");

	// for (uint8_t i = 0; i < LENGTH(board); ++i) {
		// spawn(WALL, 0, i);
		// spawn(WALL, i, 0);
		// spawn(WALL, LENGTH(board) - 1, i);
		// spawn(WALL, i, LENGTH(board) - 1);
	// }

	for (Entity *e = entities; entities->hp; ++e) {
		e = e->hp ? e : entities;
		CLASS(e).act(e);
	}
}
