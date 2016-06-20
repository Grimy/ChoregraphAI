// main.c - initialization, main game loop

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "base.h"
#include "cotton.h"
#include "cotton.c"
#include "monsters.c"
#include "xml.c"
#include UI

static void player_turn(char input) {
	if (TILE(player.pos).class == STAIRS && miniboss_defeated)
		exit(VICTORY);

	player.confusion -= SIGN(player.confusion);
	player.freeze -= SIGN(player.freeze);
	player_moved = false;

	switch (input) {
	case 'e':
		player_move(-1,  0);
		break;
	case 'f':
		player_move( 0,  1);
		break;
	case 'i':
		player_move( 1,  0);
		break;
	case 'j':
		player_move( 0, -1);
		break;
	case '<':
		bomb_plant(player.pos, 3);
		break;
	case 'z':
		break;
	default:
		fprintf(stderr, "See you soon!");
		exit(0);
	}

	if (sliding_on_ice)
		player_moved = forced_move(&player, DIRECTION(player.pos - player.prev_pos));
	else if (!player_moved && TILE(player.pos).class == FIRE)
		damage(&player, 2, false);

	sliding_on_ice = player_moved && TILE(player.pos).class == ICE
		&& can_move(&player, DIRECTION(player.pos - player.prev_pos));

	if (TILE(player.pos).class == STAIRS && miniboss_defeated)
		exit(VICTORY);
}

static void enemy_turn(Monster *m) {
	Coords d = player.pos - m->pos;
	m->confusion -= SIGN(m->confusion);
	m->freeze -= SIGN(m->freeze);
	if (!m->aggro) {
		m->aggro = can_see(m->pos);
		if (L2(d) > CLASS(m).radius && !bomb_exploded)
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
	case OMNIBOUNCE:
		forced_move(m, DIRECTION(m->pos - m->prev_pos));
		break;
	case BOUNCE:
		forced_move(m, this->dir);
		break;
	case SPIKE:
		damage(m, 4, true);
		break;
	case TRAPDOOR:
	case TELEPORT:
		monster_remove(m);
		break;
	case CONFUSE:
		if (!m->confusion)
			m->confusion = 10;
		break;
	case BOMBTRAP:
		if (m == &player)
			bomb_plant(this->pos, 2);
		break;
	case TEMPO_DOWN:
	case TEMPO_UP:
	case FIREPIG:
		break;
	}
}

// Runs one full beat of the game.
// During each beat, the player acts first, enemies second and traps last.
// Enemies act in decreasing priority order. Traps have an arbitrary order.
static void do_beat(char input) {
	player_turn(input);
	bomb_exploded = false;
	for (Monster *m = player.next; m; m = m->next)
		enemy_turn(m);
	for (Trap *t = traps; t->pos.x; ++t)
		trap_turn(t);
	++current_beat;
}

// Runs the simulation on the given custom dungeon file.
i32 main(i32 argc, char **argv) {
	if (argc != 2)
		FATAL("Usage: %s <dungeon_file.xml>", argv[0]);
	xml_parse(argv[1]);
	qsort(monsters, monster_count, sizeof(*monsters), compare_priorities);
	for (Monster *m = monsters; m->hp; ++m) {
		TILE(m->pos).monster = m;
		(m == monsters ? &player : m - 1)->next = m;
	}
	init();
}
