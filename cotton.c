#include <stdio.h>
#include <stdlib.h>

#define LENGTH(array) (sizeof(array) / sizeof(*(array)))
#define uint unsigned long

typedef struct entity {
	struct entity *next;
	int x;
	int y;
	int hp;
	int dx;
	int dy;
	int state;
	void (*act) (struct entity*);
} Entity;

static Entity *board[32][32];
static Entity entities[32];

static void display(void) {
	for (uint y = 0; y < LENGTH(board); ++y) {
		for (uint x = 0; x < LENGTH(*board); ++x) {
			putchar(board[y][x] ? '@' : '.');
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

static void move_ent(Entity *e, int dy, int dx) {
	rm_ent(e);
	e->y += dy;
	e->x += dx;
	add_ent(e);
}

static void player_input(Entity *this) {
	display();
	switch (getchar()) {
		case 'e': move_ent(this,  0, -1); break;
		case 'f': move_ent(this,  1,  0); break;
		case 'i': move_ent(this,  0,  1); break;
		case 'j': move_ent(this, -1,  0); break;
		default: break;
	}
	printf("\033[H\033[2J");
}

static void basic_seek(Entity *this) {
	if (this->state) {
		--this->state;
		return;
	}

	Entity *player = entities;
	int dy = player->y - this->y;
	int dx = player->x - this->x;

	if (!dy) {
		this->dy = 0;
		this->dx = 1;
	} else if (!dx) {
		this->dy = 1;
		this->dx = 0;
	}
	if (this->dy * dy < 0) this->dy *= -1;
	if (this->dx * dx < 0) this->dx *= -1;
	// printf("%d %d\n", this->dy, this->dx);

	if (!board[this->y + this->dy][this->x + this->dx]) {
		move_ent(this, this->dy, this->dx);
		this->state = 1;
	}
}

int main(void) {
	system("stty -echo -icanon eol \1");
	entities[0] = (Entity) {.x = 16, .y = 16, .hp = 1, .act = player_input};
	entities[1] = (Entity) {.x = 4,  .y = 4,  .dx = 1, .hp = 1, .act = basic_seek};

	for (int x = 10; x < 30; ++x)
		board[10][x] = &(Entity) { .y = 10, .x = x };

	for (Entity *e = entities; e->hp; ++e)
		add_ent(e);

	for (Entity *e = entities;; ++e) {
		e = e->hp ? e : entities;
		e->act(e);
	}
}
