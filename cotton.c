#include <stdio.h>
#include <stdlib.h>

#define LENGTH(array) ((int) (sizeof(array) / sizeof(*(array))))
#define SIGN(x) (((x) > 0) - ((x) < 0))
#define ABS(x)  ((x) < 0 ? -(x) : (x))

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
static int prev_y;
static int prev_x;

static void display();

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

		// #6: TODO

		// #7: keep moving along the same axis
		!!this->dy;

	this->dy = vertical ? SIGN(dy) : 0;
	this->dx = vertical ? 0 : SIGN(dx);

	if (can_move(this, this->dy, this->dx)) {
		move_ent(this);
		this->state = 1;
	}
}

static void display(void) {
	printf("\033[H\033[2J");
	for (int y = 0; y < LENGTH(board); ++y) {
		for (int x = 0; x < LENGTH(*board); ++x) {
			Entity *e = board[y][x];
			putchar(e ? e->act == player_input ? '@' : e->act == basic_seek ? 'Z' : '+' : '.');
		}
		putchar('\n');
	}
}

int main(void) {
	system("stty -echo -icanon eol \1");
	entities[0] = (Entity) {.x = 16, .y = 16, .hp = 1, .act = player_input};
	entities[1] = (Entity) {.x = 4,  .y = 4,  .dx = 1, .hp = 1, .act = basic_seek};

	for (int x = 10; x < 30; ++x)
		board[10][x] = &(Entity) {.hp = 0};

	for (int i = 0; i < LENGTH(board); ++i) {
		board[0][i] = &(Entity) {.hp = 0};
		board[i][0] = &(Entity) {.hp = 0};
		board[LENGTH(board) - 1][i] = &(Entity) {.hp = 0};
		board[i][LENGTH(board) - 1] = &(Entity) {.hp = 0};
	}

	for (Entity *e = entities; e->hp; ++e)
		add_ent(e);

	for (Entity *e = entities;; ++e) {
		e = e->hp ? e : entities;
		e->act(e);
	}
}
