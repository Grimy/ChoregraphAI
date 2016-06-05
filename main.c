// main.c - initialization, main game loop

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
	if (TILE(player.pos).class == FIRE)
		fire_tile = &TILE(player.pos);
	display_prompt();
	if (player.confused)
		player.confused--;
	if (&TILE(player.pos) == fire_tile)
		damage(&player, 2, false);
}

static void enemy_turn(Monster *m) {
	Coords d = player.pos - m->pos;
	if (!m->aggro) {
		m->aggro = can_see(m->pos.x, m->pos.y);
		if (L2(d) > CLASS(m).radius)
			return;
	}
	if (m->delay) {
		m->delay--;
		return;
	}
	CLASS(m).act(m, d);
}

static void trap_turn(Trap *this) {
	Monster *target = TILE(this->pos).monster;
	if (target == NULL || CLASS(target).flying)
		return;
	switch (this->class) {
		case OMNIBOUNCE: break;
		case BOUNCE: forced_move(target, this->dir); break;
		case SPIKE: damage(target, 4, true); break;
		case TRAPDOOR: damage(target, 4, true); break;
		case CONFUSE: target->confused = 9; break;
		case TELEPORT: break;
		case TEMPO_DOWN: break;
		case TEMPO_UP: break;
		case BOMBTRAP: bomb_plant(this->pos, 2);
		case FIREPIG: break;
	}
}

// Runs one full beat of the game.
// During each beat, the player acts first, enemies second and traps last.
// Enemies act in decreasing priority order. Traps have an arbitrary order.
static void do_beat(void) {
	player_turn();
	for (Monster *m = player.next; m; m = m->next)
		enemy_turn(m);
	for (Trap *t = traps; t->pos.x; ++t)
		trap_turn(t);
}

// Runs the simulation on the given custom dungeon file.
int main(int argc, char **argv) {
	if (argc != 2)
		exit(argc);
	xml_parse(argv[1]);
	qsort(monsters, monster_count, sizeof(*monsters), compare_priorities);
	for (Monster *m = monsters; m->hp; ++m) {
		TILE(m->pos).monster = m;
		(m == monsters ? &player : m - 1)->next = m;
	}
	system("stty -echo -icanon eol \1");
	while (player.hp)
		do_beat();
}
