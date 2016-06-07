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
	player.confusion -= SIGN(player.confusion);
	player.freeze -= SIGN(player.freeze);
	display_prompt();
	if (&TILE(player.pos) == fire_tile)
		damage(&player, 2, false);
}

static void enemy_turn(Monster *m) {
	Coords d = player.pos - m->pos;
	m->confusion -= SIGN(m->confusion);
	m->freeze -= SIGN(m->freeze);
	if (!m->aggro) {
		m->aggro = can_see(m->pos);
		if (L2(d) > CLASS(m).radius)
			return;
	}
	if (m->delay)
		m->delay--;
	else if (!m->freeze)
		CLASS(m).act(m, d);
}

static void trap_turn(Trap *this) {
	Monster *m = TILE(this->pos).monster;
	if (m == NULL || m->untrapped || CLASS(m).flying)
		return;
	m->untrapped = true;

	switch (this->class) {
	case OMNIBOUNCE: forced_move(m, DIRECTION(m->pos - m->prev_pos)); break;
	case BOUNCE:     forced_move(m, this->dir);                       break;
	case SPIKE:      damage(m, 4, true);                              break;
	case TRAPDOOR:   monster_remove(m);                               break;
	case CONFUSE:    if (!m->confusion) m->confusion = 10;            break;
	case TELEPORT:   monster_remove(m);                               break;
	case TEMPO_DOWN:                                                  break;
	case TEMPO_UP:                                                    break;
	case BOMBTRAP:   if (m == &player) bomb_plant(this->pos, 2);      break;
	case FIREPIG:                                                     break;
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
	while (true)
		do_beat();
}
