// monsters.c - defines all monsters in the game and their AIs

#include "chore.h"

#define MOVE(x, y) (enemy_move(m, {(x), (y)}))

const Coords plus_shape[] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}};

const Coords square_shape[] = {
	{-1, -1}, {-1, 0}, {-1, 1},
	{0, -1}, {0, 0}, {0, 1},
	{1, -1}, {1, 0}, {1, 1},
};

const Coords cone_shape[] = {
	{1, 0},
	{2, -1}, {2, 0}, {2, 1},
	{3, -2}, {3, -1}, {3, 0}, {3, 1}, {3, 2},
};

// Helpers //

// Tests whether the condition for a breath attack are met.
// This requires an unbroken line-of-sight, but can go through other monsters.
static bool can_breathe(const Monster *m, Coords d)
{
	if (m->type == BLUE_DRAGON && (abs(d.x) > 3 || abs(d.y) >= abs(d.x) || player.freeze))
		return false;
	if (m->type != BLUE_DRAGON && d.y)
		return false;

	Coords move = direction(d);
	for (Coords pos = m->pos + move; pos.x != player.pos.x; pos += move)
		if (BLOCKS_LOS(pos))
			return false;
	return true;
}

static void evaporate(Tile *tile)
{
	if (tile->type == ICE)
		tile->type = WATER;
	else if (tile->type == WATER)
		tile->type = FLOOR;
}

// Deals normal damage to all monsters on a horizontal line.
void fireball(Coords pos, i8 dir)
{
	assert(dir != 0);
	animation(FIREBALL, pos, {dir, 0});
	for (pos.x += dir; !BLOCKS_LOS(pos); pos.x += dir) {
		damage(&MONSTER(pos), 5, {dir, 0}, DMG_NORMAL);
		evaporate(&TILE(pos));
	}
}

// Freezes all monsters in a 3x5 cone.
static void cone_of_cold(Coords pos, i8 dir)
{
	animation(CONE_OF_COLD, pos, {dir, 0});
	for (Coords d: cone_shape) {
		Coords dest = pos + d * dir;
		if (dest.x >= 31 || dest.x <= 0)
			return;
		Monster *m = &MONSTER(dest);
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

	Coords move = direction(d);
	for (Coords pos = m->pos + move; pos != dest; pos += move)
		if (!IS_EMPTY(pos))
			return false;

	m->prev_pos = m->pos - direction(d);
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
	Coords vertical = {0, sign(d.y)};
	Coords horizontal = {sign(d.x), 0};

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
void explosion(Monster *m, UNUSED Coords _)
{
	animation(EXPLOSION, m->pos, {});

	if (&MONSTER(m->pos) == m)
		TILE(m->pos).monster = 0;

	for (Coords d: square_shape) {
		Tile *tile = &TILE(m->pos + d);
		evaporate(tile);
		tile->destroyed = true;
		tile->item = NO_ITEM;
		dig(m->pos + d, SHOP, false);
		damage(&MONSTER(m->pos + d), 4, d, DMG_BOMB);
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
		MOVE(-sign(d.x), 0) || MOVE(0, -1) || MOVE(0, 1);
	else if (d.x == 0)
		MOVE(0, -sign(d.y)) || MOVE(-1, 0) || MOVE(1, 0);
	else if (abs(d.y) > abs(d.x))
		MOVE(0, -sign(d.y)) || MOVE(-sign(d.x), 0);
	else
		MOVE(-sign(d.x), 0) || MOVE(0, -sign(d.y));
}

// Move diagonally toward the player. Tiebreak by *reverse* bomb-order.
static void diagonal_seek(Monster *m, Coords d)
{
	if (d.y == 0)
		MOVE(sign(d.x), 1) || MOVE(sign(d.x), -1);
	else if (d.x == 0)
		MOVE(1, sign(d.y)) || MOVE(-1, sign(d.y));
	else
		MOVE(sign(d.x), sign(d.y))
		    || MOVE(1,  sign(d.y) * -sign(d.x))
		    || MOVE(-1, sign(d.y) * sign(d.x));
}

// Move toward the player either cardinally or diagonally.
static void moore_seek(Monster *m, Coords d)
{
	if (MOVE(sign(d.x), sign(d.y)) || d.x == 0 || d.y == 0)
		return;
	if (d.x < 0)
		MOVE(-1, 0) || MOVE(0, sign(d.y));
	else
		MOVE(0, sign(d.y)) || MOVE(1, 0);
}

// Keep moving in the same direction
static void charge(Monster *m, UNUSED Coords d)
{
	Coords charging_dir = m->pos - m->prev_pos;
	if (m->type != ARMADILDO && m->type != BARREL)
		charging_dir = cardinal(charging_dir);
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
		m->dir = direction(d);
		return true;
	}
}

// Z1 //

// Move in a repeating pattern.
static void slime(Monster *m, UNUSED Coords d)
{
	static const Coords moves[][4] = {
		{{ 0, 0}, { 0,  0}, { 0,  0}, { 0,  0}}, // green
		{{ 0, 1}, { 0, -1}, { 0,  1}, { 0, -1}}, // blue
		{{ 1, 0}, { 0,  1}, {-1,  0}, { 0, -1}}, // yellow
		{{-1, 1}, { 1,  1}, { 1, -1}, {-1, -1}}, // fire
		{{ 1, 1}, {-1,  1}, {-1, -1}, { 1, -1}}, // ice
		{{-1, 1}, { 1,  0}, {-1, -1}, { 1,  0}}, // purple
		{{ 1, 0}, {-1,  1}, {-1,  0}, { 1, -1}}, // black
	};
	if (enemy_move(m, moves[m->type - GREEN_SLIME][m->state]) == MOVE_SUCCESS)
		m->state = (m->state + 1) & 3;
}

// Move in a straight line, turn around when blocked.
static void zombie(Monster *m, UNUSED Coords d)
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
	if (magic(m, d, L2(d) == 4 && can_move(m, direction(d))))
		forced_move(&player, -direction(d));
}

// Attack in a 3x3 zone without moving.
static void mushroom(Monster *m, Coords d)
{
	animation(SPORES, m->pos, {});
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

static void clone(Monster *m, UNUSED Coords d)
{
	if (g.player_moved)
		enemy_move(m, direction(player.prev_pos - player.pos));
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
	if (has_moved)
		mushroom(m, player.pos - m->pos);
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

	i8 dx = d.x ? sign(d.x) : -1;
	i8 dy = d.y ? sign(d.y) : -1;
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
	} else if (m->state == 1 && can_move(m, direction(player.prev_pos - m->prev_pos))) {
		enemy_move(m, direction(player.prev_pos - m->prev_pos));
		enemy_move(m, direction(player.prev_pos - m->prev_pos));
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
	for (Coords move: moves) {
		i64 score = L1(d - move);
		if (!score || score >= min || !can_move(m, move))
			continue;
		if ((L2(move) == 9 || L2(move) == 4)
		    && (BLOCKS_LOS(m->pos + direction(move))
		    || BLOCKS_LOS(m->pos + direction(move) * 2)))
			continue;
		if (L2(move) == 5
		    && BLOCKS_LOS(m->pos + move / 2)
		    && (BLOCKS_LOS(m->pos + direction(move))
		    || BLOCKS_LOS(m->pos + direction(move) - move / 2)))
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
	if (magic(m, d, L2(d) == 4 && can_move(m, direction(d)) && !player.confusion))
		player.confusion = 5;
}

// Spawn a random skeleton in a random direction
static void sarcophagus(Monster *m, UNUSED Coords d)
{
	static const MonsterType types[] = {SKELETON_1, SKELETANK_1, WINDMAGE_1, RIDER_1};

	if (g.monsters[g.sarco_spawn].hp || !g.seed)
		return;

	u8 spawned = types[rng()] + m->type - SARCO_1;
	monster_spawn(spawned, m->pos + random_dir(m), 1);
	g.sarco_spawn = g.last_monster;
}

static void wind_statue(UNUSED Monster *m, Coords d)
{
	if (L1(d) == 1)
		forced_move(&player, d);
}

static void bomb_statue(Monster *m, Coords d)
{
	if (m->state)
		explosion(m, d);
	else if (L1(d) == 1)
		m->state = 1;
}

static void firepig(Monster *m, Coords d)
{
	if (m->state) {
		fireball(m->pos, m->dir.x);
		m->state = 0;
		m->delay = 4;
	} else if (abs(d.x) <= 5 && sign(d.x) == m->dir.x && can_breathe(m, d)) {
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

static void orb(Monster *m, UNUSED Coords d)
{
	m->was_requeued = true;
	if (enemy_move(m, m->dir) != MOVE_SUCCESS)
		monster_kill(m, DMG_NORMAL);
}

static void wire_zombie(Monster *m, UNUSED Coords d)
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
		Coords clonk_dir = cardinal(player.prev_pos - m->pos);
		for (i8 i = 1; i <= 3; ++i) {
			Coords pos = m->pos + clonk_dir * i;
			dig(pos, SHOP, false);
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

const Monster proto[] = {
#define X(name, glyph, ai, ...) { __VA_ARGS__, name },
#include "monsters.table"
#undef X
};

void (*const monster_ai[])(Monster*, Coords) = {
#define X(name, glyph, ai, ...) ai,
#include "monsters.table"
#undef X
};
