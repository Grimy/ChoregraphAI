// monsters.c - defines all monsters in the game and their AIs

#include "chore.h"

#define MOVE(x, y) (enemy_move(m, {(x), (y)}))

// Helpers //

// Tests whether the condition for a breath attack are met.
// This requires an unbroken line-of-sight, but can go through other monsters.
static bool can_breathe(const Monster *m, Coords d)
{
	if (m->type == BLUE_DRAGON && (abs(d.x) > 3 || abs(d.y) >= abs(d.x) || player.freeze))
		return false;
	if (m->type != BLUE_DRAGON && d.y)
		return false;

	Coords move = DIRECTION(d);
	for (Coords pos = m->pos + move; pos.x != player.pos.x; pos += move)
		if (BLOCKS_LOS(pos))
			return false;
	return true;
}

// Deals normal damage to all monsters on a horizontal line.
void fireball(Coords pos, i8 dir)
{
	assert(dir != 0);
	for (pos.x += dir; !BLOCKS_LOS(pos); pos.x += dir)
		damage(&MONSTER(pos), 5, {dir, 0}, DMG_NORMAL);
}

// Freezes all monsters in a 3x5 cone.
static void cone_of_cold(Coords pos, i8 dir)
{
	static const Coords cone_shape[] = {
		{1, 0},
		{2, -1}, {2, 0}, {2, 1},
		{3, -2}, {3, -1}, {3, 0}, {3, 1}, {3, 2},
	};
	for (u64 i = 0; i < ARRAY_SIZE(cone_shape); ++i) {
		if (pos.x + dir * cone_shape[i].x >= 32)
			return;
		Monster *m = &MONSTER(pos + cone_shape[i] * dir);
		m->freeze = 4 + (m == &player);
	}
}

// Tests whether a monster can charge toward the given position.
// This requires a straight path without walls nor monsters.
// /!\ side-effect: sets prev_pos
static bool _can_charge(Monster *m, Coords dest)
{
	Coords d = dest - m->pos;
	if (d.x * d.y != 0 && !(m->type == ARMADILDO && abs(d.x) == abs(d.y)))
		return false;

	Coords move = DIRECTION(d);
	for (Coords pos = m->pos + move; pos != dest; pos += move)
		if (!IS_EMPTY(pos))
			return false;

	m->prev_pos = m->pos - DIRECTION(d);
	return true;
}

// Tests whether a monster can charge at the player’s current or previous position.
static bool can_charge(Monster *m)
{
	return _can_charge(m, player.pos) ||
		(g.player_moved && _can_charge(m, player.prev_pos));
}

// Returns the direction a basic_seek enemy would move in.
// Helper function for basic_seek and its variants.
static Coords seek_dir(const Monster *m, Coords d)
{
	// Ignore the player’s previous position if they moved more than one tile
	Coords prev_pos = L1(player.pos - player.prev_pos) > 1 ? player.pos : player.prev_pos;
	Coords vertical = {0, SIGN(d.y)};
	Coords horizontal = {SIGN(d.x), 0};

	bool axis =
		// #1: move toward the player
		d.y == 0 ? 0 :
		d.x == 0 ? 1 :

		// #2: avoid obstacles
		!can_move(m, vertical) ?
		!can_move(m, horizontal) && abs(d.y) > abs(d.x) :
		!can_move(m, horizontal) ? 1 :

		// #3: move toward the player’s previous position
		m->pos.y == prev_pos.y ? 0 :
		m->pos.x == prev_pos.x ? 1 :

		// #4: weird edge cases
		m->prev_pos.y == player.pos.y ? 0 :
		m->prev_pos.x == player.pos.x ? 1 :
		m->prev_pos.y == prev_pos.y ? abs(d.x) == 1 :
		m->prev_pos.x == prev_pos.x ? abs(d.y) != 1 :

		// #5: keep moving along the same axis
		m->dir.y != 0;

	return axis ? vertical : horizontal;
}

// Rolls a random integer from 0 to 3, using a simple xorshift RNG.
static u32 rng() {
	g.seed ^= g.seed << 13;
	g.seed ^= g.seed >> 17;
	g.seed ^= g.seed << 5;
	return g.seed & 3;
}

// Returns a random cardinal direction.
// We first try at random up to 4 times, then try all directions in order.
// This is slightly biased, but it’s very fast, never fails to move when
// possible, and never gets stuck in an infinite loop.
static Coords random_dir(Monster *m)
{
	for (u64 i = 0; i < 8; ++i) {
		Coords dir = plus_shape[i < 4 ? rng() : i & 3];
		if (IS_EMPTY(m->pos + dir))
			return dir;
	}
	return {0, 0};
}

// Common //

// Explode, damaging the 9 surrounding tiles in the following order:
// 147
// 258
// 369
// This is called “bomb-order”. Many monster AIs use it as a tiebreaker:
// when several destination tiles fit all criteria equally well, monsters
// pick the one that comes first in bomb-order.
void bomb_detonate(Monster *m, __attribute__((unused)) Coords d)
{
	static const Coords square_shape[] = {
		{-1, -1}, {-1, 0}, {-1, 1},
		{0, -1}, {0, 0}, {0, 1},
		{1, -1}, {1, 0}, {1, 1},
	};

	if (&MONSTER(m->pos) == m)
		TILE(m->pos).monster = 0;

	for (i64 i = 0; i < 9; ++i) {
		Tile *tile = &TILE(m->pos + square_shape[i]);
		tile->type = tile->type == WATER ? FLOOR : tile->type == ICE ? WATER : tile->type;
		dig(m->pos + square_shape[i], 4, false);
		damage(&MONSTER(m->pos + square_shape[i]), 4, square_shape[i], DMG_BOMB);
		tile->destroyed = true;
		tile->item = NO_ITEM;
	}

	m->hp = 0;
}

// Move cardinally toward the player, avoiding obstacles.
// Try to keep moving along the same axis, unless the monster’s current or
// previous position is aligned with the player’s current or previous position.
static void basic_seek(Monster *m, Coords d)
{
	m->dir = seek_dir(m, d);
	enemy_move(m, m->dir);
}

// Move away from the player.
// Tiebreak by L2 distance, then bomb-order.
static void basic_flee(Monster *m, Coords d)
{
	if (d.y == 0)
		MOVE(-SIGN(d.x), 0) || MOVE(0, -1) || MOVE(0, 1);
	else if (d.x == 0)
		MOVE(0, -SIGN(d.y)) || MOVE(-1, 0) || MOVE(1, 0);
	else if (abs(d.y) > abs(d.x))
		MOVE(0, -SIGN(d.y)) || MOVE(-SIGN(d.x), 0);
	else
		MOVE(-SIGN(d.x), 0) || MOVE(0, -SIGN(d.y));
}

// Move diagonally toward the player. Tiebreak by *reverse* bomb-order.
static void diagonal_seek(Monster *m, Coords d)
{
	if (d.y == 0)
		MOVE(SIGN(d.x), 1) || MOVE(SIGN(d.x), -1);
	else if (d.x == 0)
		MOVE(1, SIGN(d.y)) || MOVE(-1, SIGN(d.y));
	else
		MOVE(SIGN(d.x), SIGN(d.y))
		    || MOVE(1,  SIGN(d.y) * -SIGN(d.x))
		    || MOVE(-1, SIGN(d.y) * SIGN(d.x));
}

// Move toward the player either cardinally or diagonally.
static void moore_seek(Monster *m, Coords d)
{
	if (MOVE(SIGN(d.x), SIGN(d.y)) || d.x == 0 || d.y == 0)
		return;
	if (d.x < 0)
		MOVE(-1, 0) || MOVE(0, SIGN(d.y));
	else
		MOVE(0, SIGN(d.y)) || MOVE(1, 0);
}

// Keep moving in the same direction
static void charge(Monster *m, __attribute__((unused)) Coords d)
{
	Coords charging_dir = m->pos - m->prev_pos;
	if (m->type != ARMADILDO && m->type != BARREL)
		charging_dir = CARDINAL(charging_dir);
	if (enemy_move(m, charging_dir) != MOVE_SUCCESS)
		m->state = 0;
	if (m->type != BARREL)
		m->prev_pos = m->pos - charging_dir;
}

// Common code for wind-mages, liches and electro-liches.
// None of them can cast while stuck in water/tar or confused.
static bool magic(Monster *m, Coords d, bool condition)
{
	if (!condition || m->confusion || IS_BOGGED(m)) {
		basic_seek(m, d);
		return false;
	} else {
		m->dir = DIRECTION(d);
		return true;
	}
}

// Z1 //

// Move in a repeating pattern.
static void slime(Monster *m, __attribute__((unused)) Coords d)
{
	static const Coords moves[][4] = {
		{{ 1, 0}, {-1,  1}, {-1,  0}, { 1, -1}}, // black
		{{ 0, 0}, { 0,  0}, { 0,  0}, { 0,  0}}, // green
		{{ 0, 1}, { 0, -1}, { 0,  1}, { 0, -1}}, // blue
		{{ 1, 0}, { 0,  1}, {-1,  0}, { 0, -1}}, // yellow
		{{-1, 1}, { 1,  0}, {-1, -1}, { 1,  0}}, // purple
		{{ 0, 0}, { 0,  0}, { 0,  0}, { 0,  0}}, // unassigned
		{{-1, 1}, { 1,  1}, { 1, -1}, {-1, -1}}, // fire
		{{ 1, 1}, {-1,  1}, {-1, -1}, { 1, -1}}, // ice
	};
	if (enemy_move(m, moves[m->type & 7][m->state]) == MOVE_SUCCESS)
		m->state = (m->state + 1) & 3;
}

// Move in a straight line, turn around when blocked.
static void zombie(Monster *m, __attribute__((unused)) Coords d)
{
	if (enemy_move(m, m->dir) < MOVE_ATTACK && !m->requeued)
		m->dir = -m->dir;
}

// Move in a random direction.
static void bat(Monster *m, Coords d)
{
	if (m->confusion)
		basic_seek(m, d);
	else if (!g.seed)
		monster_kill(m, DMG_NORMAL);
	else
		enemy_move(m, random_dir(m));
}

// Move in a randomer direction.
static void green_bat(Monster *m, Coords d)
{
	static const Coords diagonals[] = {{1, -1}, {1, 1}, {-1, -1}, {-1, 1}};
	if (rng() & 1)
		bat(m, d);
	else
		enemy_move(m, diagonals[rng()]);
}

// Move toward the player when they’re retreating, stand still otherwise.
static void ghost(Monster *m, Coords d)
{
	m->state = (L1(d) + m->state) > L1(player.prev_pos - m->pos);
	if (m->state)
		basic_seek(m, d);
}

// Z2 //

static void wind_mage(Monster *m, Coords d)
{
	if (magic(m, d, L2(d) == 4 && can_move(m, DIRECTION(d))))
		forced_move(&player, -DIRECTION(d));
}

// Attack in a 3x3 zone without moving.
static void mushroom(Monster *m, Coords d)
{
	if (L2(d) < 4)
		damage(&player, m->damage, d, DMG_NORMAL);
}

// State 0: passive
// State 1: charging
static void armadillo(Monster *m, Coords d)
{
	if (m->state == 0)
		m->state = can_charge(m);
	if (m->state == 1) {
		charge(m, d);
		m->delay = m->state ? 0 : 2;
	}
}

static void clone(Monster *m, __attribute__((unused)) Coords d)
{
	if (g.player_moved)
		enemy_move(m, DIRECTION(player.prev_pos - player.pos));
}

static void tarmonster(Monster *m, Coords d)
{
	if (m == &g.monsters[g.monkeyed])
		damage(&player, m->damage, d, DMG_NORMAL);
	else if (m->state || L1(d) > 1)
		basic_seek(m, d);
	m->state = m->state ? 2 : L1(d) == 1;
}

static void mole(Monster *m, Coords d)
{
	if (m->state != (L1(d) == 1))
		m->state ^= 1;
	else
		basic_seek(m, d);
	TILE(m->pos).destroyed = true;
}

// Z3 //

static void assassin(Monster *m, Coords d)
{
	m->state = L1(d) == 1 || (L1(d) + m->state) > L1(player.prev_pos - m->pos);
	(m->state ? basic_seek : basic_flee)(m, d);
}

// Chase the player, then attack in a 3x3 zone.
static void yeti(Monster *m, Coords d)
{
	bool has_moved = m->pos != m->prev_pos;
	basic_seek(m, d);
	if (has_moved && L2(player.pos - m->pos) < 4)
		damage(&player, m->damage, d, DMG_NORMAL);
}

// Z4 //

static void digger(Monster *m, Coords d)
{
	if (m->state == 0) {
		if (L2(d) <= 9) {
			m->state = 1;
			m->delay = 3;
		}
		return;
	}

	i8 dx = d.x ? SIGN(d.x) : -1;
	i8 dy = d.y ? SIGN(d.y) : -1;
	Coords moves[4] = {{-dx, 0}, {0, -dy}, {0, dy}, {dx, 0}};
	Coords move = {0, 0};
	bool vertical = abs(d.y) > (abs(d.x) + 1) / 3;

	for (i64 i = 0; i < 3; ++i) {
		move = moves[i ^ vertical];
		if (!TILE(m->pos + move).monster && TILE(m->pos + move).type != EDGE)
			break;
	}

	if (enemy_move(m, move) < MOVE_ATTACK)
		m->delay = 3;
}

// Attack the player if possible, otherwise move randomly.
static void black_bat(Monster *m, Coords d)
{
	if (L1(d) == 1)
		enemy_move(m, d);
	else
		bat(m, d);
}

// After parrying a melee hit, lunge two tiles in the direction the hit came from.
static void blademaster(Monster *m, Coords d)
{
	if (m->state == 2) {
		m->state = 0;
	} else if (m->state == 1 && can_move(m, DIRECTION(player.prev_pos - m->prev_pos))) {
		enemy_move(m, DIRECTION(player.prev_pos - m->prev_pos));
		enemy_move(m, DIRECTION(player.prev_pos - m->prev_pos));
		m->delay = 0;
		m->state = 2;
		m->requeued = false;
	} else {
		basic_seek(m, d);
	}
}

// Move up to 3 tiles toward the player, but only attack if the player is adjacent.
// Only move to visible tiles. Move as little as possible in L2 distance.
//    N
//   IFJ
//  HDBDK
// MEA.CGO
//  IDBDL
//   JFK
//    N
static void harpy(Monster *m, Coords d)
{
	static const Coords moves[] = {
		{-1, 0}, {0, -1}, {0, 1}, {1, 0},
		{-1, -1}, {-1, 1}, {1, -1}, {1, 1},
		{-2, 0}, {0, -2}, {0, 2}, {2, 0},
		{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1},
		{-3, 0}, {0, -3}, {0, 3}, {3, 0},
	};
	if (L1(d) == 1) {
		enemy_move(m, d);
		return;
	}
	Coords best_move = {0, 0};
	i64 min = L1(d);
	for (u64 i = 0; i < ARRAY_SIZE(moves); ++i) {
		Coords move = moves[i];
		i64 score = L1(d - move);
		if (!score || score >= min || !can_move(m, move))
			continue;
		if ((L2(move) == 9 || L2(move) == 4)
		    && (BLOCKS_LOS(m->pos + DIRECTION(move))
		    || BLOCKS_LOS(m->pos + DIRECTION(move) * 2)))
			continue;
		if (L2(move) == 5
		    && BLOCKS_LOS(m->pos + move / 2)
		    && (BLOCKS_LOS(m->pos + DIRECTION(move))
		    || BLOCKS_LOS(m->pos + DIRECTION(move) - move / 2)))
			continue;
		min = score;
		best_move = move;
		if (score == 1 || score == L1(d) - 3)
			break;
	}
	enemy_move(m, best_move);
}

static void lich(Monster *m, Coords d)
{
	if (magic(m, d, L2(d) == 4 && can_move(m, DIRECTION(d)) && !player.confusion))
		player.confusion = 5;
}

// Spawn a random skeleton in a random direction
static void sarcophagus(Monster *m, __attribute__((unused)) Coords d)
{
	static const MonsterType types[] = {SKELETON_1, SKELETANK_1, WINDMAGE_1, RIDER_1};

	if (g.monsters[g.sarco_spawn].hp || !g.seed)
		return;

	u8 spawned = types[rng()] + m->type - SARCO_1;
	monster_spawn(spawned, m->pos + random_dir(m), 1);
	g.sarco_spawn = g.last_monster;
}

static void wind_statue(__attribute__((unused)) Monster *m, Coords d)
{
	if (L1(d) == 1)
		forced_move(&player, d);
}

static void bomb_statue(Monster *m, Coords d)
{
	if (m->state)
		bomb_detonate(m, d);
	else if (L1(d) == 1)
		m->state = 1;
}

static void firepig(Monster *m, Coords d)
{
	if (m->state) {
		fireball(m->pos, m->dir.x);
		m->state = 0;
		m->delay = 4;
	} else if (abs(d.x) <= 5 && SIGN(d.x) == m->dir.x && can_breathe(m, d)) {
		m->state = 1;
	}
}

// Z5 //

static void electro_lich(Monster *m, Coords d)
{
	Coords true_prev_pos = m->prev_pos;

	if (magic(m, d, L1(d) > 1 && can_charge(m))) {
		m->dir = m->pos - m->prev_pos;
		u8 spawned = m->type + (ORB_1 - ELECTRO_1);
		monster_spawn(spawned, m->pos + m->dir, 0)->dir = m->dir;
	}

	m->prev_pos = true_prev_pos;
}

static void orb(Monster *m, __attribute__((unused)) Coords d)
{
	m->was_requeued = true;
	if (enemy_move(m, m->dir) != MOVE_SUCCESS)
		monster_kill(m, DMG_NORMAL);
}

static void wire_zombie(Monster *m, __attribute__((unused)) Coords d)
{
	if (enemy_move(m, m->dir) < MOVE_ATTACK && !m->requeued)
		m->dir = -m->dir;

	m->delay = IS_WIRE(m->pos) ? 0 : 1;

	if (m->delay == 0 && !TILE(m->pos + m->dir).wired) {
		Coords right = { -m->dir.y, m->dir.x };
		Coords left = { m->dir.y, -m->dir.x };
		if (TILE(m->pos + right).wired ^ TILE(m->pos + left).wired) {
			m->dir = TILE(m->pos + right).wired ? right : left;
		} else {
			m->dir = -m->dir;
			m->delay = 1;
		}
	}
}

static void evil_eye(Monster *m, Coords d)
{
	if (!m->state) {
		m->state = L1(d) <= 3 && _can_charge(m, player.pos);
		m->dir = m->pos - m->prev_pos;
		return;
	}
	m->state = 0;
	if (enemy_move(m, m->dir)) {
		m->was_requeued = true;
		enemy_move(m, m->dir) == MOVE_SUCCESS && enemy_move(m, m->dir);
	}
}

static void orc(Monster *m, Coords d)
{
	Coords old_dir = m->dir;
	m->dir = seek_dir(m, d);
	if (m->dir == old_dir)
		basic_seek(m, d);
	else
		m->prev_pos = m->pos;
}

static void devil(Monster *m, Coords d)
{
	moore_seek(m, d);
	if (m->state)
		m->delay = 0;
}

// State 0: camouflaged
// State 1: invulnerable (right after waking up)
// State 2: chasing the player
static void mimic(Monster *m, Coords d)
{
	if (m->state) {
		m->state = 2;
		basic_seek(m, d);
	} else if (L1(d) == 1) {
		m->state = 1;
	}
}

// Like a mimic, but can also move or wake up diagonally.
static void moore_mimic(Monster *m, Coords d)
{
	if (m->state) {
		m->state = 2;
		moore_seek(m, d);
	} else if (L2(d) <= 2) {
		m->state = 1;
	}
}

// Minibosses

// Dragons normally chase the player cardinally every two beats (see basic_seek).
// However, as soon as the player is in breath range, they’ll charge a breath attack,
// then fire it on the next beat.
// They then resume chasing, but can’t charge another breath in the next two beats.
static void dragon(Monster *m, Coords d)
{
	i8 direction = 1;

	switch (m->state) {
	case 0:
		m->dir = seek_dir(m, d);
		m->state = enemy_move(m, m->dir) != MOVE_FAIL;
		break;
	case 1:
		m->state = 0;
		break;
	case 2:
		direction = -1;
		[[clang::fallthrough]];
	case 3:
		(m->type == RED_DRAGON ? fireball : cone_of_cold)(m->pos, direction);
		m->exhausted = 3;
		m->state = 1;
		break;
	}

	if (!m->exhausted && can_breathe(m, player.pos - m->pos))
		m->state = 2 + (d.x > 0);
}

// Charge the player when possible, otherwise default to basic_seek.
static void minotaur(Monster *m, Coords d)
{
	armadillo(m, d);
	if (m->state == 0 && m->delay == 0)
		basic_seek(m, d);
}

// Move toward the player, then spawn a mummy on the empty tile closest
// to the tile immediately below (L2 distance, then bomb-order).
// ..K..
// .E@F.
// IBACJ
// .GDH.
static void mommy(Monster *m, Coords d)
{
	static const Coords spawn_dirs[] = {
		{0, 1}, {-1, 1}, {1, 1}, {0, 2}, {-1, 0}, {1, 0},
		{-1, 2}, {1, 2}, {-2, 1}, {2, 1}, {0, -1},
	};
	const Coords *spawn_dir;

	bool has_moved = m->pos != m->prev_pos;
	basic_seek(m, d);

	if (has_moved && !g.monsters[g.mommy_spawn].hp) {
		for (spawn_dir = spawn_dirs; !IS_EMPTY(m->pos + *spawn_dir); ++spawn_dir);
		monster_spawn(MUMMY, m->pos + *spawn_dir, 1);
		g.mommy_spawn = g.last_monster;
	}
}

// Can clonk on either the third or fourth beat of his 4 beat cycle.
// After clonking, attacks on a 3-tile long line, destroying walls.
// State 0 = 4th beat (can move or clonk)
// State 1 = 3rd beat (can clonk, but not move).
// State 2 = clonking
static void ogre(Monster *m, Coords d)
{
	if (m->state == 2) {
		Coords clonk_dir = CARDINAL(player.prev_pos - m->pos);
		for (i8 i = 1; i <= 3; ++i) {
			Coords pos = m->pos + clonk_dir * i;
			dig(pos, 4, false);
			damage(&MONSTER(pos), 5, clonk_dir, DMG_NORMAL);
		}
		m->state = 1;
		m->delay = 2;
	} else if (d.x * d.y == 0 && abs(d.x + d.y) <= 3) {
		// Clonk!
		m->state = 2;
	} else if (m->state == 1) {
		m->state = 0;
	} else {
		basic_seek(m, d);
		m->state = 1;
		m->delay = 2;
	}
}

// Move three times, then attack like a mushroom
static void metrognome(Monster *m, Coords d)
{
	m->state = (m->state + 1) & 3;
	(m->state ? basic_seek : mushroom)(m, d);
}

const TypeInfos type_infos[] = {
	// [Name] = { damage, max_hp, beat_delay, flying, radius, dig, priority, glyph, act }
	[GREEN_SLIME]  = { 99, 1, 0,  false, 999, -1, 63, GREEN "P",  slime },
	[BLUE_SLIME]   = {  2, 2, 1,  false, 999, -1, 32, BLUE "P",   slime },
	[YELLOW_SLIME] = {  1, 1, 0,  false, 999, -1, 14, YELLOW "P", slime },
	[SKELETON_1]   = {  1, 1, 1,  false,   9, -1, 18, "Z",        basic_seek },
	[SKELETON_2]   = {  3, 2, 1,  false,   9, -1, 40, YELLOW "Z", basic_seek },
	[SKELETON_3]   = {  4, 3, 1,  false,   9, -1, 55, BLACK "Z",  basic_seek },
	[BLUE_BAT]     = {  1, 1, 1,   true,   9, -1, 18, BLUE "B",   bat },
	[RED_BAT]      = {  2, 1, 0,   true,   9, -1, 24, RED "B",    bat },
	[GREEN_BAT]    = {  3, 1, 0,   true,   9, -1, 36, GREEN "B",  green_bat },
	[MONKEY_1]     = {  0, 1, 0,  false,   9, -1, 10, PURPLE "Y", basic_seek },
	[MONKEY_2]     = {  0, 2, 0,  false,   9, -1, 12, "Y",        basic_seek },
	[GHOST]        = {  2, 1, 0,   true,   9, -1, 23, "8",        ghost },
	[ZOMBIE]       = {  2, 1, 1,  false, 999, -1, 25, GREEN "Z",  zombie },
	[WRAITH]       = {  1, 1, 0,   true,   9, -1, 14, RED "W",    basic_seek },
	[MIMIC_1]      = {  2, 1, 0,  false,   1, -1, 22, ORANGE "m", mimic },
	[MIMIC_2]      = {  3, 1, 0,  false,   1, -1, 33, BLUE "m",   mimic },
	[MIMIC_3]      = {  2, 1, 0,  false,   1, -1, 22, ORANGE "m", mimic },
	[MIMIC_4]      = {  2, 1, 0,  false,   1, -1, 22, ORANGE "m", mimic },
	[MIMIC_5]      = {  2, 1, 0,  false,   1, -1, 22, ORANGE "m", mimic },
	[WHITE_MIMIC]  = {  3, 1, 0,  false,   1, -1, 33, "m",        mimic },
	[HEADLESS_2]   = {  3, 1, 0,  false,   0, -1,  0, YELLOW "∠", charge },
	[HEADLESS_3]   = {  4, 1, 0,  false,   0, -1,  0, BLACK "∠",  charge },

	[SKELETANK_1]  = {  1, 1, 1,  false,  25, -1, 18, "Ź",        basic_seek },
	[SKELETANK_2]  = {  3, 2, 1,  false,  25, -1, 41, YELLOW "Ź", basic_seek },
	[SKELETANK_3]  = {  5, 3, 1,  false,  25, -1, 60, BLACK "Ź",  basic_seek },
	[WINDMAGE_1]   = {  2, 1, 1,  false,   0, -1, 26, BLUE "@",   wind_mage },
	[WINDMAGE_2]   = {  4, 2, 1,  false,   0, -1, 52, YELLOW "@", wind_mage },
	[WINDMAGE_3]   = {  5, 3, 1,  false,   0, -1, 60, BLACK "@",  wind_mage },
	[MUSHROOM_1]   = {  2, 1, 3,  false,  25, -1, 28, BLUE "F",   mushroom },
	[MUSHROOM_2]   = {  4, 3, 2,  false,  25, -1, 56, PURPLE "F", mushroom },
	[GOLEM_1]      = {  4, 5, 3,   true,  25,  2, 66, "'",        basic_seek },
	[GOLEM_2]      = {  6, 7, 3,   true,  25,  2, 68, BLACK "'",  basic_seek },
	[ARMADILLO_1]  = {  2, 1, 0,  false,  -1,  2, 23, "q",        armadillo },
	[ARMADILLO_2]  = {  3, 2, 0,  false,  -1,  2, 39, YELLOW "q", armadillo },
	[CLONE]        = {  3, 1, 0,  false,  25, -1, 35, "@",        clone },
	[TARMONSTER]   = {  3, 1, 0,   true,   1, -1, 44, "t",        tarmonster },
	[MOLE]         = {  2, 1, 0,   true,  25, -1,  3, "r",        mole },
	[WIGHT]        = {  2, 1, 0,   true,  25, -1, 24, GREEN "W",  basic_seek },
	[WALL_MIMIC]   = {  2, 1, 0,  false,   1, -1, 24, GREEN "m",  mimic },
	[LIGHTSHROOM]  = {  0, 1, 0,  false,   0, -1,  0, "F",        NULL },
	[BOMBSHROOM]   = {  4, 1, 0,  false,  25, -1, 86, YELLOW "F", NULL },
	[BOMBSHROOM_]  = {  4, 1, 0,  false,  25, -1, 86, RED "F",    bomb_detonate },

	[FIRE_SLIME]   = {  3, 1, 0,  false, 999,  2, 34, RED "P",    slime },
	[ICE_SLIME]    = {  3, 1, 0,  false, 999,  2, 34, CYAN "P",   slime },
	[RIDER_1]      = {  2, 1, 0,   true,  49, -1, 23, "&",        basic_seek },
	[RIDER_2]      = {  4, 2, 0,   true,  49, -1, 51, YELLOW "&", basic_seek },
	[RIDER_3]      = {  6, 3, 0,   true,  49, -1, 62, BLACK "&",  basic_seek },
	[EFREET]       = {  3, 2, 2,   true,  49,  2, 65, RED "E",    basic_seek },
	[DJINN]        = {  3, 2, 2,   true,  49,  2, 65, CYAN "E",   basic_seek },
	[ASSASSIN_1]   = {  4, 1, 0,  false,  49, -1, 47, PURPLE "o", assassin },
	[ASSASSIN_2]   = {  6, 2, 0,  false,  49, -1, 61, BLACK "o",  assassin },
	[FIRE_BEETLE]  = {  3, 3, 1,  false,  49, -1, 43, RED "a",    basic_seek },
	[ICE_BEETLE]   = {  3, 3, 1,  false,  49, -1, 43, CYAN "a",   basic_seek },
	[BEETLE]       = {  3, 3, 1,  false,  49, -1, 43, "a",        basic_seek },
	[HELLHOUND]    = {  3, 1, 1,  false,  49, -1, 37, RED "d",    moore_seek },
	[SHOVE_1]      = {  0, 2, 0,  false,  49, -1,  6, PURPLE "~", basic_seek },
	[SHOVE_2]      = {  0, 3, 0,  false,  49, -1,  9, BLACK "~",  basic_seek },
	[YETI]         = {  3, 1, 3,   true,  49,  2, 64, CYAN "Y",   yeti },
	[GHAST]        = {  2, 1, 0,   true,  49, -1, 23, PURPLE "W", basic_seek },
	[FIRE_MIMIC]   = {  2, 1, 0,  false,   1, -1, 23, RED "m",    mimic },
	[ICE_MIMIC]    = {  2, 1, 0,  false,   1, -1, 23, CYAN "m",   mimic },
	[FIRE_POT]     = {  0, 1, 0,  false,   0, -1,  0, RED "(",    NULL },
	[ICE_POT]      = {  0, 1, 0,  false,   0, -1,  0, CYAN "(",   NULL },

	[BOMBER]       = {  4, 1, 1,  false,   0, -1, 84, RED "o",    diagonal_seek },
	[DIGGER]       = {  1, 1, 0,  false,   9,  2, 17, "o",        digger },
	[BLACK_BAT]    = {  2, 1, 0,   true,   9, -1, 48, BLACK "B",  black_bat },
	[ARMADILDO]    = {  3, 3, 0,  false,  -1,  2, 42, ORANGE "q", armadillo },
	[BLADENOVICE]  = {  1, 1, 1,  false,   9, -1, 81, "b",        blademaster },
	[BLADEMASTER]  = {  2, 2, 1,  false,   9, -1, 82, YELLOW "b", blademaster },
	[GHOUL]        = {  1, 1, 0,   true,   9, -1, 35, "W",        moore_seek },
	[GOOLEM]       = {  5, 5, 3,   true,   9,  2, 67, GREEN "'",  basic_seek },
	[HARPY]        = {  3, 1, 1,   true,   0, -1, 38, GREEN "h",  harpy },
	[LICH_1]       = {  2, 1, 1,  false,   0, -1, 57, GREEN "L",  lich },
	[LICH_2]       = {  3, 2, 1,  false,   0, -1, 58, PURPLE "L", lich },
	[LICH_3]       = {  4, 3, 1,  false,   0, -1, 59, BLACK "L",  lich },
	[CONF_MONKEY]  = {  0, 1, 0,  false,   9, -1, 11, GREEN "Y",  basic_seek },
	[TELE_MONKEY]  = {  0, 2, 0,  false,   9, -1,  7, PINK "Y",   basic_seek },
	[PIXIE]        = {  4, 1, 0,   true,   0, -1, 46, "n",        basic_seek },
	[SARCO_1]      = {  0, 1, 7,  false,   9, -1, 19, "|",        sarcophagus },
	[SARCO_2]      = {  0, 2, 9,  false,   9, -1, 20, YELLOW "|", sarcophagus },
	[SARCO_3]      = {  0, 3, 11, false,   9, -1, 21, BLACK "|",  sarcophagus },
	[SPIDER]       = {  2, 1, 1,  false,   9, -1, 49, YELLOW "s", basic_seek },
	[FREE_SPIDER]  = {  2, 1, 0,  false,   9, -1, 49, RED "s",    diagonal_seek },
	[WARLOCK_1]    = {  3, 1, 1,  false,   9, -1, 49, "w",        basic_seek },
	[WARLOCK_2]    = {  4, 2, 1,  false,   9, -1, 50, YELLOW "w", basic_seek },
	[MUMMY]        = {  2, 1, 1,  false,   9, -1, 69, "M",        moore_seek },
	[WIND_STATUE]  = {  4, 1, 0,  false,   0, -1, 46, CYAN "g",   wind_statue },
	[MIMIC_STATUE] = {  4, 1, 0,  false,   1, -1, 46, BLACK "g",  mimic },
	[BOMB_STATUE]  = {  4, 1, 0,  false,   0, -1, 46, YELLOW "g", bomb_statue },
	[MINE_STATUE]  = {  4, 1, 0,  false,   0, -1,  0, RED "g",    NULL },
	[CRATE_1]      = {  0, 1, 0,  false,   0, -1,  0, "(",        NULL },
	[CRATE_2]      = {  0, 1, 0,  false,   0, -1,  0, "(",        NULL },
	[BARREL]       = {  1, 1, 0,  false, 999,  1,  2, BROWN "(",  charge },
	[TEH_URN]      = {  0, 3, 0,  false,   0, -1,  0, PURPLE "(", NULL },
	[CHEST]        = {  0, 1, 0,  false,   0, -1,  0, BLACK "(",  NULL },
	[FIREPIG]      = {  5, 1, 0,  false,   0, -1,  1, RED "q",    firepig },

	[SKULL_1]      = {  1, 1, 1,  false,   9, -1, 16, WHITE "z",  basic_seek },
	[SKULL_2]      = {  2, 1, 1,  false,   9, -1, 31, YELLOW "z", basic_seek },
	[SKULL_3]      = {  4, 1, 1,  false,   9, -1, 54, BLACK "z",  basic_seek },
	[WATER_BALL]   = {  0, 1, 0,   true,   9, -1,  4, BLUE "e",   moore_seek },
	[TAR_BALL]     = {  0, 1, 0,   true,   9, -1,  5, BLACK "e",  moore_seek },
	[ELECTRO_1]    = {  1, 1, 1,  false,   0, -1, 18, BLUE "L",   electro_lich },
	[ELECTRO_2]    = {  3, 2, 1,  false,   0, -1, 40, RED "L",    electro_lich },
	[ELECTRO_3]    = {  4, 3, 1,  false,   0, -1, 55, YELLOW "L", electro_lich },
	[ORB_1]        = {  1, 1, 0,   true, 999,  1, 13, YELLOW "e", orb },
	[ORB_2]        = {  3, 1, 0,   true, 999,  1, 33, YELLOW "e", orb },
	[ORB_3]        = {  4, 1, 0,   true, 999,  1, 45, YELLOW "e", orb },
	[GORGON_1]     = {  0, 1, 0,  false,   9, -1,  5, GREEN "S",  basic_seek },
	[GORGON_2]     = {  0, 3, 0,  false,   9, -1,  8, YELLOW "S", basic_seek },
	[WIRE_ZOMBIE]  = {  2, 1, 0,  false, 999, -1, 25, ORANGE "Z", wire_zombie },
	[EVIL_EYE_1]   = {  1, 1, 0,   true,   0,  2, 15, GREEN "e",  evil_eye },
	[EVIL_EYE_2]   = {  2, 2, 0,   true,   0,  2, 30, PINK "e",   evil_eye },
	[ORC_1]        = {  1, 1, 0,  false,   9, -1, 14, GREEN "o",  orc },
	[ORC_2]        = {  2, 2, 0,  false,   9, -1, 29, PINK "o",   orc },
	[ORC_3]        = {  3, 3, 0,  false,   9, -1, 42, PURPLE "o", orc },
	[DEVIL_1]      = {  2, 1, 2,  false,   9, -1, 27, RED "&",    devil },
	[DEVIL_2]      = {  4, 2, 2,  false,   9, -1, 53, GREEN "&",  devil },
	[PURPLE_SLIME] = {  3, 1, 0,  false, 999, -1, 35, PURPLE "P", slime },
	[BLACK_SLIME]  = {  3, 1, 0,  false, 999, -1, 35, BLACK "P",  slime },
	[WHITE_SLIME]  = {  3, 1, 0,  false, 999, -1, 35, "P",        slime },
	[CURSE]        = {  0, 1, 0,   true,   9, -1,  5, YELLOW "W", basic_seek },
	[SHOP_MIMIC]   = {  2, 1, 0,  false,   2, -1, 22, YELLOW "m", moore_mimic },
	[STONE_STATUE] = {  0, 1, 0,  false,   0, -1,  0, BLACK "S",  NULL },
	[GOLD_STATUE]  = {  0, 3, 0,  false,   0, -1,  0, BLACK "S",  NULL },

	[DIREBAT_1]    = {  3, 2, 1,   true,   9, -1, 70, YELLOW "B", bat },
	[DIREBAT_2]    = {  4, 3, 1,   true,   9, -1, 74, "B",        bat },
	[DRAGON]       = {  4, 4, 1,   true,  49,  4, 75, GREEN "D",  basic_seek },
	[RED_DRAGON]   = {  6, 6, 0,   true, 100,  4, 85, RED "D",    dragon },
	[BLUE_DRAGON]  = {  6, 6, 0,   true,   0,  4, 83, BLUE "D",   dragon },
	[EARTH_DRAGON] = {  6, 8, 1,   true,   0,  4, 80, BROWN "D",  basic_seek },
	[BANSHEE_1]    = {  4, 3, 0,   true,  25, -1, 71, BLUE "8",   basic_seek },
	[BANSHEE_2]    = {  6, 4, 0,   true,   9, -1, 79, GREEN "8",  basic_seek },
	[MINOTAUR_1]   = {  4, 3, 0,   true,  49,  2, 71, "H",        minotaur },
	[MINOTAUR_2]   = {  5, 5, 0,   true,  49,  2, 77, BLACK "H",  minotaur },
	[NIGHTMARE_1]  = {  4, 3, 1,   true,  81,  4, 73, BLACK "u",  basic_seek },
	[NIGHTMARE_2]  = {  5, 5, 1,   true,  81,  4, 78, RED "u",    basic_seek },
	[MOMMY]        = {  4, 6, 3,   true,   9, -1, 76, BLACK "@",  mommy },
	[OGRE]         = {  5, 5, 0,   true,   9,  2, 77, GREEN "O",  ogre },
	[METROGNOME_1] = {  4, 3, 0,   true,   9,  1, 72, YELLOW "G", metrognome },
	[METROGNOME_2] = {  5, 5, 0,   true,   9,  1, 77, GREEN "G",  metrognome },

	[NO_MONSTER]   = {  0, 0, 0,   true,   0, -1,  0, NULL,       NULL },
	[SHOPKEEPER]   = { 20, 9, 0,  false,   0, -1, 83, "@",        NULL },
	[PLAYER]       = {  1, 1, 0,  false,   2,  1, 99, "@",        NULL },
	[BOMB]         = {  4, 1, 0,  false, 999, -1, 98, ORANGE "●", bomb_detonate },
	[SHRINE]       = {  0, 1, 0,  false,   0, -1,  0, "_",        NULL },
};
