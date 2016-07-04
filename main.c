// main.c - initialization, main game loop

#include "base.h"
#include "chore.h"
#include "utils.c"
#include "monsters.c"
#include "xml.c"
#include UI

static void player_turn(u8 input)
{
	player.confusion -= SIGN(player.confusion);
	player.freeze -= SIGN(player.freeze);
	g.player_moved = false;

	switch (input) {
	case 0:
		player_move(-1,  0);
		break;
	case 1:
		player_move( 0,  1);
		break;
	case 2:
		player_move( 1,  0);
		break;
	case 3:
		player_move( 0, -1);
		break;
	case 4:
		if (g.player_bombs-- > 0)
			bomb_plant(player.pos, 3);
		break;
	case 5:
		g.boots_on ^= 1;
		break;
	}

	if (g.sliding_on_ice)
		g.player_moved = forced_move(&player, DIRECTION(player.pos - player.prev_pos));
	else if (!g.player_moved && TILE(player.pos).class == FIRE)
		damage(&player, 2, NO_DIR, DMG_NORMAL);

	g.sliding_on_ice = g.player_moved && TILE(player.pos).class == ICE
		&& can_move(&player, DIRECTION(player.pos - player.prev_pos));
}

static void enemy_turn(Monster *m)
{
	Coords d = player.pos - m->pos;
	m->confusion -= SIGN(m->confusion);
	m->freeze -= SIGN(m->freeze);

	// The bomb-aggro bug
	if (!m->aggro && (g.bomb_exploded || (nightmare && L2(m->pos - nightmare->pos) < 9)))
		m->aggro = can_see(m->pos);

	if (!m->aggro) {
		m->aggro = can_see(m->pos);
		if (!(m->aggro && (m->delay || m->class == BLUE_DRAGON)) && L2(d) > CLASS(m).radius)
			return;
	} else if (m->class >= SARCO_1 && m->class <= SARCO_3) {
		g.sarco_on = true;
	}

	if (m->delay)
		m->delay--;
	else if (!m->freeze)
		CLASS(m).act(m, d);
}

static void trap_turn(Trap *this)
{
	if (TILE(this->pos).traps_destroyed)
		return;

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
		damage(m, 4, NO_DIR, DMG_NORMAL);
		break;
	case TRAPDOOR:
	case TELEPORT:
		monster_kill(m, DMG_NORMAL);
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
		break;
	}
}

// Runs one full beat of the game.
// During each beat, the player acts first, enemies second and traps last.
// Enemies act in decreasing priority order. Traps have an arbitrary order.
static void do_beat(u8 input)
{
	Monster *m = g.monsters;
	++g.current_beat;
	g.bomb_exploded = false;

	player_turn(input);
	if (player_won())
		return;
	update_fov();

	for (; CLASS(m).act; ++m) {
		if (m->hp <= 0)
			continue;
		u32 old_state = m->state;
		enemy_turn(m);
		if (m->requeued)
			m->state = old_state;
	}

	while (--m >= g.monsters)
		if (m->requeued)
			CLASS(m).act(m, player.pos - m->pos);

	for (Trap *t = g.traps; t->pos.x; ++t)
		trap_turn(t);
}

// Runs the simulation on the given custom dungeon file.
i32 main(i32 argc, char **argv) {
	if (argc < 2)
		FATAL("Usage: %s dungeon_file.xml [level]", argv[0]);
	xml_parse(argv[1], argc == 3 ? *argv[2] - '0' : 1);
	update_fov();
	run();
}
