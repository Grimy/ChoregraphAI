// main.c - initialization, main game loop

#define SPAWN_Y 9
#define SPAWN_X 24

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cotton.h"
#include "cotton.c"
#include "ui.c"
#include "monsters.c"
#include "xml.c"

static void player_turn() {
	Tile *fire_tile = NULL;
	if (board[player.y][player.x].class == FIRE)
		fire_tile = &board[player.y][player.x];
	display_prompt();
	if (&board[player.y][player.x] == fire_tile)
		player.hp = 0;
}

static void enemy_turn(Monster *m) {
	long dy = player.y - m->y;
	long dx = player.x - m->x;
	m->aggro = m->aggro || can_see(m->y, m->x);
	if (!m->aggro && dy * dy + dx * dx > CLASS(m).radius)
		return;
	if (m->delay) {
		m->delay--;
		return;
	}
	CLASS(m).act(m, dy, dx);
}

static void trap_turn(Trap *this) {
	Monster *target = board[this->y][this->x].monster;
	if (target == NULL || CLASS(target).flying)
		return;
	switch (this->class) {
		case OMNIBOUNCE: break;
		case BOUNCE: forced_move(target, this->dy, this->dx); break;
		case SPIKE: damage(target, 4, true); break;
		case TRAPDOOR: damage(target, 4, true); break;
		case CONFUSE: break;
		case TELEPORT: break;
		case TEMPO_DOWN: break;
		case TEMPO_UP: break;
		case BOMBTRAP: break;
		case FIREPIG: break;
	}
}

// Runs one full beat of the game.
// During each beat, the player acts first, enemies second and traps last.
// Enemies act in decreasing priority order. Traps have an arbitrary order.
static void do_beat(void) {
	player_turn();
	for (Monster *m = monsters; m->y; ++m)
		if (m->hp > 0)
			enemy_turn(m);
	for (Trap *t = traps; t->y; ++t)
		trap_turn(t);
}

// Runs the simulation on the given custom dungeon file.
int main(int argc, char **argv) {
	if (argc != 2)
		exit(argc);
	xml_parse(argv[1]);
	qsort(monsters, monster_count, sizeof(*monsters), compare_priorities);
	for (Monster *m = monsters; m->x; ++m)
		board[m->y][m->x].monster = m;
	system("stty -echo -icanon eol \1");
	while (player.hp)
		do_beat();
}
