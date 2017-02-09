// monsters.c - defines all monsters in the game and their AIs

#include "chore.h"

#define MOVE(x, y) (enemy_move(m, (Coords) {(x), (y)}))

// Many things in the game follow the so-called “bomb order”:
// 147
// 258
// 369
// Most monster AIs use m as a tiebreaker: when several destination tiles fit
// all criteria equally well, monsters pick the one that comes first in bomb-order.

// Helpers //

// Tests whether the condition for a breath attack are met.
// This requires an unbroken line-of-sight, but can go through other monsters.
static bool can_breathe(Monster *m, Coords d)
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

// Tests whether the given monster can charge toward the given position.
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

// Tests whether the given monster can charge at the player’s current
// or previous position.
static bool can_charge(Monster *m)
{
	return _can_charge(m, player.pos) ||
		(g.player_moved && _can_charge(m, player.prev_pos));
}

// The direction a basic_seek enemy would move in.
// Helper function for basic_seek and its variants.
static Coords seek_dir(Monster *m, Coords d)
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

// Common //

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
// If the chosen direction is blocked, cycles through the other directions
// in the order right > left > down > up (mnemonic: Ryan Loves to Dunk Us).
static void bat(Monster *m, Coords d)
{
	static const Coords moves[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	if (m->confusion) {
		basic_seek(m, d);
		return;
	}
	if (!g.seed) {
		monster_kill(m, DMG_NORMAL);
		return;
	}
	i64 rng = RNG();
	for (i64 i = 0; i < 4; ++i)
		if (enemy_move(m, moves[(rng + i) & 3]))
			return;
}

// Move in a randomer direction.
static void green_bat(Monster *m, Coords d)
{
	static const Coords moves[] = {
		{1, 0}, {-1, 0}, {0, 1}, {0, -1},
		{1, -1}, {1, 1}, {-1, -1}, {-1, 1},
	};
	if (m->confusion) {
		basic_seek(m, d);
		return;
	}
	if (!g.seed) {
		monster_kill(m, DMG_NORMAL);
		return;
	}
	i64 rng = RNG() * 2 + RNG();
	for (i64 i = 0; i < 8; ++i)
		if (enemy_move(m, moves[(rng + i) & 7]))
			return;
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

static void beetle_shed(Monster *m)
{
	tile_change(m->pos, m->type == FIRE_BEETLE ? FIRE : ICE);
	m->type = BEETLE;
}

static void beetle(Monster *m, Coords d)
{
	if (L1(d) == 1)
		beetle_shed(m);
	basic_seek(m, d);
	if (L1(player.pos - m->pos) == 1)
		beetle_shed(m);
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

	Coords moves[4] = {{-SIGN(d.x), 0}, {0, -SIGN(d.y)}, {0, SIGN(d.y)}, {SIGN(d.x), 0}};
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

// Unlike bats, sarcophagi aren’t biased.
static void sarcophagus(Monster *m, __attribute__((unused)) Coords d)
{
	static const MonsterType types[] = {SKELETON_1, SKELETANK_1, WINDMAGE_1, RIDER_1};

	if (g.monsters[g.sarco_spawn].hp || !g.seed)
		return;

	// Try at random up to 4 times, then try in order
	for (u64 i = 0; i < 8; ++i) {
		Coords spawn_dir = plus_shape[i < 4 ? RNG() : i & 3];
		if (!IS_EMPTY(m->pos + spawn_dir))
			continue;
		u8 spawned = types[RNG()] + m->type - SARCO_1;
		monster_spawn(spawned, m->pos + spawn_dir, 1);
		g.sarco_spawn = g.last_monster;
		return;
	}
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
			destroy_wall(pos);
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

const TypeInfos type_infos[256] = {
	// [Name] = { damage, max_hp, beat_delay, radius, flying, dig, priority, glyph, act }
	[GREEN_SLIME]  = { 99, 1, 0, 999, false, -1, 19901101, GREEN "P",  slime },
	[BLUE_SLIME]   = {  2, 2, 1, 999, false, -1, 10202202, BLUE "P",   slime },
	[YELLOW_SLIME] = {  1, 1, 0, 999, false, -1, 10101102, YELLOW "P", slime },
	[SKELETON_1]   = {  1, 1, 1,   9, false, -1, 10101202, "Z",        basic_seek },
	[SKELETON_2]   = {  3, 2, 1,   9, false, -1, 10302203, YELLOW "Z", basic_seek },
	[SKELETON_3]   = {  4, 3, 1,   9, false, -1, 10403204, BLACK "Z",  basic_seek },
	[BLUE_BAT]     = {  1, 1, 1,   9,  true, -1, 10101202, BLUE "B",   bat },
	[RED_BAT]      = {  2, 1, 0,   9,  true, -1, 10201103, RED "B",    bat },
	[GREEN_BAT]    = {  3, 1, 0,   9,  true, -1, 10301120, GREEN "B",  green_bat },
	[MONKEY_1]     = {  0, 1, 0,   9, false, -1, 10004101, PURPLE "Y", basic_seek },
	[MONKEY_2]     = {  0, 2, 0,   9, false, -1, 10006103, "Y",        basic_seek },
	[GHOST]        = {  2, 1, 0,   9,  true, -1, 10201102, "8",        ghost },
	[ZOMBIE]       = {  2, 1, 1, 999, false, -1, 10201201, GREEN "Z",  zombie },
	[WRAITH]       = {  1, 1, 0,   9,  true, -1, 10101102, RED "W",    basic_seek },
	[MIMIC_1]      = {  2, 1, 0,   1, false, -1, 10201100, ORANGE "m", mimic },
	[MIMIC_2]      = {  3, 1, 0,   1, false, -1, 10301100, BLUE "m",   mimic },
	[MIMIC_3]      = {  2, 1, 0,   1, false, -1, 10201100, ORANGE "m", mimic },
	[MIMIC_4]      = {  2, 1, 0,   1, false, -1, 10201100, ORANGE "m", mimic },
	[MIMIC_5]      = {  2, 1, 0,   1, false, -1, 10201100, ORANGE "m", mimic },
	[WHITE_MIMIC]  = {  3, 1, 0,   1, false, -1, 10301100, "m",        mimic },
	[HEADLESS]     = {  1, 1, 0,   0, false, -1, 10302203, "∠",        charge },

	[SKELETANK_1]  = {  1, 1, 1,  25, false, -1, 10101202, "Ź",        basic_seek },
	[SKELETANK_2]  = {  3, 2, 1,  25, false, -1, 10302204, YELLOW "Ź", basic_seek },
	[SKELETANK_3]  = {  5, 3, 1,  25, false, -1, 10503206, BLACK "Ź",  basic_seek },
	[WINDMAGE_1]   = {  2, 1, 1,   0, false, -1, 10201202, BLUE "@",   wind_mage },
	[WINDMAGE_2]   = {  4, 2, 1,   0, false, -1, 10402204, YELLOW "@", wind_mage },
	[WINDMAGE_3]   = {  5, 3, 1,   0, false, -1, 10503206, BLACK "@",  wind_mage },
	[MUSHROOM_1]   = {  2, 1, 3,  25, false, -1, 10201402, BLUE "%",   mushroom },
	[MUSHROOM_2]   = {  4, 3, 2,  25, false, -1, 10403303, PURPLE "%", mushroom },
	[GOLEM_1]      = {  4, 5, 3,  25,  true,  2, 20405404, "'",        basic_seek },
	[GOLEM_2]      = {  6, 7, 3,  25,  true,  2, 20607407, BLACK "'",  basic_seek },
	[ARMADILLO_1]  = {  2, 1, 0,  -1, false,  2, 10201102, "q",        armadillo },
	[ARMADILLO_2]  = {  3, 2, 0,  -1, false,  2, 10302105, YELLOW "q", armadillo },
	[CLONE]        = {  3, 1, 0,  25, false, -1, 10301102, "@",        clone },
	[TARMONSTER]   = {  3, 1, 0,   1,  true, -1, 10304103, "t",        tarmonster },
	[MOLE]         = {  2, 1, 0,  25,  true, -1,  1020113, "r",        mole },
	[WIGHT]        = {  2, 1, 0,  25,  true, -1, 10201103, GREEN "W",  basic_seek },
	[WALL_MIMIC]   = {  2, 1, 0,   1, false, -1, 10201103, GREEN "m",  mimic },
	[LIGHTSHROOM]  = {  0, 1, 0,   0, false, -1,        0, "%",        NULL },
	[BOMBSHROOM]   = {  4, 1, 0,   0, false, -1,      ~2u, YELLOW "%", NULL },
	[BOMBSHROOM_]  = {  4, 1, 0, 999, false, -1,      ~2u, RED "%",    bomb_detonate },

	[FIRE_SLIME]   = {  3, 1, 0, 999, false,  2, 10301101, RED "P",    slime },
	[ICE_SLIME]    = {  3, 1, 0, 999, false,  2, 10301101, CYAN "P",   slime },
	[RIDER_1]      = {  2, 1, 0,  49,  true, -1, 10201102, "&",        basic_seek },
	[RIDER_2]      = {  4, 2, 0,  49,  true, -1, 10402104, YELLOW "&", basic_seek },
	[RIDER_3]      = {  6, 3, 0,  49,  true, -1, 10603106, BLACK "&",  basic_seek },
	[EFREET]       = {  3, 2, 2,  49,  true,  2, 20302302, RED "E",    basic_seek },
	[DJINN]        = {  3, 2, 2,  49,  true,  2, 20302302, CYAN "E",   basic_seek },
	[ASSASSIN_1]   = {  4, 1, 0,  49, false, -1, 10401103, PURPLE "o", assassin },
	[ASSASSIN_2]   = {  6, 2, 0,  49, false, -1, 10602105, BLACK "o",  assassin },
	[FIRE_BEETLE]  = {  3, 3, 1,  49, false, -1, 10303202, RED "a",    beetle },
	[ICE_BEETLE]   = {  3, 3, 1,  49, false, -1, 10303202, CYAN "a",   beetle },
	[BEETLE]       = {  3, 3, 1,  49, false, -1, 10303202, "a",        basic_seek },
	[HELLHOUND]    = {  3, 1, 1,  49, false, -1, 10301202, RED "d",    moore_seek },
	[SHOVE_1]      = {  0, 2, 0,  49, false, -1, 10002102, PURPLE "~", basic_seek },
	[SHOVE_2]      = {  0, 3, 0,  49, false, -1, 10003102, BLACK "~",  basic_seek },
	[YETI]         = {  3, 1, 3,  49,  true,  2, 20301403, CYAN "Y",   yeti },
	[GHAST]        = {  2, 1, 0,  49,  true, -1, 10201102, PURPLE "W", basic_seek },
	[FIRE_MIMIC]   = {  2, 1, 0,   1, false, -1, 10201102, RED "m",    mimic },
	[ICE_MIMIC]    = {  2, 1, 0,   1, false, -1, 10201102, CYAN "m",   mimic },
	[FIRE_POT]     = {  0, 1, 0,   0, false, -1,        0, RED "(",    NULL },
	[ICE_POT]      = {  0, 1, 0,   0, false, -1,        0, CYAN "(",   NULL },

	[BOMBER]       = {  4, 1, 1,   0, false, -1, 99999998, RED "o",    diagonal_seek },
	[DIGGER]       = {  1, 1, 0,   9, false,  2, 10101201, "o",        digger },
	[BLACK_BAT]    = {  2, 1, 0,   9,  true, -1, 10401120, BLACK "B",  black_bat },
	[ARMADILDO]    = {  3, 3, 0,  -1, false,  2, 10303104, ORANGE "q", armadillo },
	[BLADENOVICE]  = {  1, 1, 1,   9, false, -1, 99999995, "b",        blademaster },
	[BLADEMASTER]  = {  2, 2, 1,   9, false, -1, 99999996, YELLOW "b", blademaster },
	[GHOUL]        = {  1, 1, 0,   9,  true, -1, 10301102, "W",        moore_seek },
	[GOOLEM]       = {  5, 5, 3,   9,  true,  2, 20510407, GREEN "'",  basic_seek },
	[HARPY]        = {  3, 1, 1,   0,  true, -1, 10301203, GREEN "h",  harpy },
	[LICH_1]       = {  2, 1, 1,   0, false, -1, 10404202, GREEN "L",  lich },
	[LICH_2]       = {  3, 2, 1,   0, false, -1, 10404302, PURPLE "L", lich },
	[LICH_3]       = {  4, 3, 1,   0, false, -1, 10404402, BLACK "L",  lich },
	[CONF_MONKEY]  = {  0, 1, 0,   9, false, -1, 10004103, GREEN "Y",  basic_seek },
	[TELE_MONKEY]  = {  0, 2, 0,   9, false, -1, 10002103, PINK "Y",   basic_seek },
	[PIXIE]        = {  4, 1, 0,   0,  true, -1, 10401102, "n",        basic_seek },
	[SARCO_1]      = {  0, 1, 7,   9, false, -1, 10101805, "|",        sarcophagus },
	[SARCO_2]      = {  0, 2, 9,   9, false, -1, 10102910, YELLOW "|", sarcophagus },
	[SARCO_3]      = {  0, 3, 11,  9, false, -1, 10103915, BLACK "|",  sarcophagus },
	[SPIDER]       = {  2, 1, 1,   9, false, -1, 10401202, YELLOW "s", basic_seek },
	[FREE_SPIDER]  = {  2, 1, 0,   9, false, -1, 10401202, RED "s",    diagonal_seek },
	[WARLOCK_1]    = {  3, 1, 1,   9, false, -1, 10401202, "w",        basic_seek },
	[WARLOCK_2]    = {  4, 2, 1,   9, false, -1, 10401302, YELLOW "w", basic_seek },
	[MUMMY]        = {  2, 1, 1,   9, false, -1, 30201103, "M",        moore_seek },
	[WIND_STATUE]  = {  4, 1, 0,   0, false, -1, 10401102, CYAN "g",   wind_statue },
	[MIMIC_STATUE] = {  4, 1, 0,   1, false, -1, 10401102, BLACK "g",  mimic },
	[BOMB_STATUE]  = {  4, 1, 0,   0, false, -1, 10401102, YELLOW "g", bomb_statue },
	[MINE_STATUE]  = {  4, 1, 0,   0, false, -1,        0, RED "g",    NULL },
	[CRATE_1]      = {  0, 1, 0,   0, false, -1,        0, "(",        NULL },
	[CRATE_2]      = {  0, 1, 0,   0, false, -1,        0, "(",        NULL },
	[BARREL]       = {  1, 1, 0, 999, false,  1,       80, BROWN "(",  charge },
	[TEH_URN]      = {  0, 3, 0,   0, false, -1,        0, PURPLE "(", NULL },
	[CHEST]        = {  0, 1, 0,   0, false, -1,        0, BLACK "(",  NULL },
	[FIREPIG]      = {  5, 1, 0,   0, false, -1,        1, RED "q",    firepig },

	[SKULL_1]      = {  1, 1, 1,   9, false, -1, 10101200, WHITE "z",  basic_seek },
	[SKULL_2]      = {  2, 1, 1,   9, false, -1, 10202200, YELLOW "z", basic_seek },
	[SKULL_3]      = {  4, 1, 1,   9, false, -1, 10403200, BLACK "z",  basic_seek },
	[WATER_BALL]   = {  0, 1, 0,   9,  true, -1, 10001101, BLUE "e",   moore_seek },
	[TAR_BALL]     = {  0, 1, 0,   9,  true, -1, 10001102, BLACK "e",  moore_seek },
	[ELECTRO_1]    = {  1, 1, 1,   0, false, -1, 10101202, BLUE "L",   electro_lich },
	[ELECTRO_2]    = {  3, 2, 1,   0, false, -1, 10302203, RED "L",    electro_lich },
	[ELECTRO_3]    = {  4, 3, 1,   0, false, -1, 10403204, YELLOW "L", electro_lich },
	[ORB_1]        = {  1, 1, 0, 999,  true,  1, 10101100, YELLOW "e", orb },
	[ORB_2]        = {  3, 1, 0, 999,  true,  1, 10301100, YELLOW "e", orb },
	[ORB_3]        = {  4, 1, 0, 999,  true,  1, 10401100, YELLOW "e", orb },
	[GORGON_1]     = {  0, 1, 0,   9, false, -1, 10001102, GREEN "S",  basic_seek },
	[GORGON_2]     = {  0, 3, 0,   9, false, -1, 10003100, YELLOW "S", basic_seek },
	[WIRE_ZOMBIE]  = {  2, 1, 0, 999, false, -1, 10201201, ORANGE "Z", wire_zombie },
	[EVIL_EYE_1]   = {  1, 1, 0,   0,  true,  2, 10101103, GREEN "e",  evil_eye },
	[EVIL_EYE_2]   = {  2, 2, 0,   0,  true,  2, 10202105, PINK "e",   evil_eye },
	[ORC_1]        = {  1, 1, 0,   9, false, -1, 10101102, GREEN "o",  orc },
	[ORC_2]        = {  2, 2, 0,   9, false, -1, 10202103, PINK "o",   orc },
	[ORC_3]        = {  3, 3, 0,   9, false, -1, 10303104, PURPLE "o", orc },
	[DEVIL_1]      = {  2, 1, 2,   9, false, -1, 10201303, RED "&",    devil },
	[DEVIL_2]      = {  4, 2, 2,   9, false, -1, 10402305, GREEN "&",  devil },
	[PURPLE_SLIME] = {  3, 1, 0, 999, false, -1, 10301102, PURPLE "P", slime },
	[BLACK_SLIME]  = {  3, 1, 0, 999, false, -1, 10301102, BLACK "P",  slime },
	[WHITE_SLIME]  = {  3, 1, 0, 999, false, -1, 10301102, "P",        slime },
	[CURSE]        = {  0, 1, 0,   9,  true, -1, 10001102, YELLOW "W", basic_seek },
	[SHOP_MIMIC]   = {  2, 1, 0,   2, false, -1, 10201100, YELLOW "m", moore_mimic },
	[STONE_STATUE] = {  0, 1, 0,   0, false, -1,        0, BLACK "S",  NULL },
	[GOLD_STATUE]  = {  0, 3, 0,   0, false, -1,        0, BLACK "S",  NULL },

	[DIREBAT_1]    = {  3, 2, 1,   9,  true, -1, 30302210, YELLOW "B", bat },
	[DIREBAT_2]    = {  4, 3, 1,   9,  true, -1, 30403215, "B",        bat },
	[DRAGON]       = {  4, 4, 1,  49,  true,  4, 30404210, GREEN "D",  basic_seek },
	[RED_DRAGON]   = {  6, 6, 0, 100,  true,  4, 99999999, RED "D",    dragon },
	[BLUE_DRAGON]  = {  6, 6, 0,   0,  true,  4, 99999997, BLUE "D",   dragon },
	[EARTH_DRAGON] = {  6, 8, 1,   0,  true,  4, 30608215, BROWN "D",  basic_seek },
	[BANSHEE_1]    = {  4, 3, 0,  25,  true, -1, 30403110, BLUE "8",   basic_seek },
	[BANSHEE_2]    = {  6, 4, 0,   9,  true, -1, 30604115, GREEN "8",  basic_seek },
	[MINOTAUR_1]   = {  4, 3, 0,  49,  true,  2, 30403110, "H",        minotaur },
	[MINOTAUR_2]   = {  5, 5, 0,  49,  true,  2, 30505115, BLACK "H",  minotaur },
	[NIGHTMARE_1]  = {  4, 3, 1,  81,  true,  4, 30403210, BLACK "u",  basic_seek },
	[NIGHTMARE_2]  = {  5, 5, 1,  81,  true,  4, 30505215, RED "u",    basic_seek },
	[MOMMY]        = {  4, 6, 3,   9,  true, -1, 30405215, BLACK "@",  mommy },
	[OGRE]         = {  5, 5, 0,   9,  true,  2, 30505115, GREEN "O",  ogre },
	[METROGNOME_1] = {  4, 3, 0,   9,  true,  1, 30403115, YELLOW "G", metrognome },
	[METROGNOME_2] = {  5, 5, 0,   9,  true,  1, 30505115, GREEN "G",  metrognome },

	[NO_MONSTER]   = {  0, 0, 0,   0,  true, -1,        0, NULL,       NULL },
	[SHOPKEEPER]   = { 20, 9, 0,   0, false, -1, 99999997, "@",        NULL },
	[PLAYER]       = {  1, 1, 0,   0, false, -1,      ~0u, "@",        NULL },
	[BOMB]         = {  4, 1, 0, 999, false, -1,      ~1u, ORANGE "●", bomb_detonate },
	[SHRINE]       = {  0, 1, 0,   0, false, -1,        0, "_",        NULL },
};
