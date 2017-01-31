// monsters.c - defines all monsters in the game and their AIs

#include "chore.h"

#define MOVE(x, y) (enemy_move(this, (Coords) {(x), (y)}))

// Many things in the game follow the so-called “bomb order”:
// 147
// 258
// 369
// Most monster AIs use this as a tiebreaker: when several destination tiles fit
// all criteria equally well, monsters pick the one that comes first in bomb-order.

// The direction a basic_seek enemy would move in.
// Helper function for basic_seek and its variants.
static Coords seek_dir(Monster *this, Coords d)
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
		!can_move(this, vertical) ?
		!can_move(this, horizontal) && abs(d.y) > abs(d.x) :
		!can_move(this, horizontal) ? 1 :

		// #3: move toward the player’s previous position
		this->pos.y == prev_pos.y ? 0 :
		this->pos.x == prev_pos.x ? 1 :

		// #4: weird edge cases
		this->prev_pos.y == player.pos.y ? 0 :
		this->prev_pos.x == player.pos.x ? 1 :
		this->prev_pos.y == prev_pos.y ? abs(d.x) == 1 :
		this->prev_pos.x == prev_pos.x ? abs(d.y) != 1 :

		// #5: keep moving along the same axis
		this->dir.y != 0;

	return axis ? vertical : horizontal;
}

// Move cardinally toward the player, avoiding obstacles.
// Try to keep moving along the same axis, unless the monster’s current or
// previous position is aligned with the player’s current or previous position.
static void basic_seek(Monster *this, Coords d)
{
	this->dir = seek_dir(this, d);
	enemy_move(this, this->dir);
}

// Move away from the player.
// Tiebreak by L2 distance, then bomb-order.
static void basic_flee(Monster *this, Coords d)
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
static void diagonal_seek(Monster *this, Coords d)
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
static void moore_seek(Monster *this, Coords d)
{
	if (MOVE(SIGN(d.x), SIGN(d.y)) || d.x == 0 || d.y == 0)
		return;
	if (d.x < 0)
		MOVE(-1, 0) || MOVE(0, SIGN(d.y));
	else
		MOVE(0, SIGN(d.y)) || MOVE(1, 0);
}

// Keep moving in the same direction
static void charge(Monster *this, __attribute__((unused)) Coords d)
{
	Coords charging_dir = this->pos - this->prev_pos;
	if (this->class != ARMADILDO && this->class != BARREL)
		charging_dir = CARDINAL(charging_dir);
	if (enemy_move(this, charging_dir) != MOVE_SUCCESS)
		this->state = 0;
	if (this->class != BARREL)
		this->prev_pos = this->pos - charging_dir;
}

// Move in a random direction.
// If the chosen direction is blocked, cycles through the other directions
// in the order right > left > down > up (mnemonic: Ryan Loves to Dunk Us).
static void bat(Monster *this, Coords d)
{
	static const Coords moves[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	if (this->confused) {
		basic_seek(this, d);
		return;
	}
	if (!g.seed) {
		monster_kill(this, DMG_NORMAL);
		return;
	}
	i64 rng = RNG();
	for (i64 i = 0; i < 4; ++i)
		if (enemy_move(this, moves[(rng + i) & 3]))
			return;
}

// Move in a randomer direction.
static void green_bat(Monster *this, Coords d)
{
	static const Coords moves[] = {
		{1, 0}, {-1, 0}, {0, 1}, {0, -1},
		{1, -1}, {1, 1}, {-1, -1}, {-1, 1},
	};
	if (this->confused) {
		basic_seek(this, d);
		return;
	}
	if (!g.seed) {
		monster_kill(this, DMG_NORMAL);
		return;
	}
	i64 rng = RNG() * 2 + RNG();
	for (i64 i = 0; i < 8; ++i)
		if (enemy_move(this, moves[(rng + i) & 7]))
			return;
}

// Attack the player if possible, otherwise move randomly.
static void black_bat(Monster *this, Coords d)
{
	if (L1(d) == 1)
		enemy_move(this, d);
	else
		bat(this, d);
}

// After parrying a melee hit, lunge two tiles in the direction the hit came from.
static void blademaster(Monster *this, Coords d)
{
	if (this->state == 2) {
		this->state = 0;
	} else if (this->state == 1 && can_move(this, DIRECTION(player.prev_pos - this->prev_pos))) {
		enemy_move(this, DIRECTION(player.prev_pos - this->prev_pos));
		enemy_move(this, DIRECTION(player.prev_pos - this->prev_pos));
		this->delay = 0;
		this->state = 2;
		this->requeued = false;
	} else {
		basic_seek(this, d);
	}
}

// Common AI for liches and windmages.
static void mage(Monster *this, Coords d)
{
	bool is_lich = this->class >= LICH_1 && this->class <= LICH_3;
	bool can_cast = L2(d) == 4
		&& can_move(this, DIRECTION(d))
		&& !(this->confused)
		&& !STUCK(this)
		&& !(player.confused && is_lich);

	if (!can_cast) {
		basic_seek(this, d);
		return;
	}

	this->delay = 1;
	if (is_lich)
		player.confusion = g.current_beat + 5;
	else
		forced_move(&player, -DIRECTION(d));
}

// Attack in a 3x3 zone without moving.
static void mushroom(Monster *this, Coords d)
{
	if (L2(d) < 4)
		enemy_attack(this);
	this->delay = CLASS(this).beat_delay;
}

// Chase the player, then attack in a 3x3 zone.
static void yeti(Monster *this, Coords d)
{
	bool has_moved = L1((this)->pos - (this)->prev_pos) != 0;
	basic_seek(this, d);
	if (has_moved && L2(player.pos - this->pos) < 4)
		enemy_attack(this);
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
static void harpy(Monster *this, Coords d)
{
	static const Coords moves[] = {
		{-1, 0}, {0, -1}, {0, 1}, {1, 0},
		{-1, -1}, {-1, 1}, {1, -1}, {1, 1},
		{-2, 0}, {0, -2}, {0, 2}, {2, 0},
		{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1},
		{-3, 0}, {0, -3}, {0, 3}, {3, 0},
	};
	if (L1(d) == 1) {
		enemy_move(this, d);
		return;
	}
	Coords best_move = {0, 0};
	i64 min = L1(d);
	for (u64 i = 0; i < ARRAY_SIZE(moves); ++i) {
		Coords move = moves[i];
		i64 score = L1(d - move);
		if (!score || score >= min || !can_move(this, move))
			continue;
		if ((L2(move) == 9 || L2(move) == 4)
		    && (BLOCKS_LOS(this->pos + DIRECTION(move))
		    || BLOCKS_LOS(this->pos + 2*DIRECTION(move))))
			continue;
		if (L2(move) == 5
		    && BLOCKS_LOS(this->pos + move / 2)
		    && (BLOCKS_LOS(this->pos + DIRECTION(move))
		    || BLOCKS_LOS(this->pos + DIRECTION(move) - move / 2)))
			continue;
		min = score;
		best_move = move;
		if (score == 1 || score == L1(d) - 3)
			break;
	}
	enemy_move(this, best_move);
}

static void zombie(Monster *this, __attribute__((unused)) Coords d)
{
	if (enemy_move(this, this->dir) < MOVE_ATTACK && !this->requeued) {
		this->dir = -this->dir;
		this->delay = 1;
	}
}

static void slime(Monster *this, __attribute__((unused)) Coords d)
{
	static const Coords moves[][4] = {
		{{ 0,  0}, { 0, 0}, { 0,  0}, { 0,  0}}, // unassigned
		{{ 0,  0}, { 0, 0}, { 0,  0}, { 0,  0}}, // green
		{{ 0, -1}, { 0, 1}, { 0, -1}, { 0,  1}}, // blue
		{{ 1,  0}, { 0, 1}, {-1,  0}, { 0, -1}}, // yellow
		{{ 0,  0}, { 0, 0}, { 0,  0}, { 0,  0}}, // unassigned
		{{-1,  1}, { 1, 0}, {-1, -1}, { 1,  0}}, // purple
		{{-1,  1}, { 1, 1}, { 1, -1}, {-1, -1}}, // fire
		{{ 1,  1}, {-1, 1}, {-1, -1}, { 1, -1}}, // ice
	};
	this->state += enemy_move(this, moves[this->class & 7][this->state]) == MOVE_SUCCESS;
	this->state &= 3;
}

// State 0: camouflaged
// State 1: invulnerable (right after waking up)
// State 2: chasing the player
static void mimic(Monster *this, Coords d)
{
	if (this->state) {
		this->state = 2;
		basic_seek(this, d);
	} else if (L1(d) == 1) {
		this->state = 1;
	}
}

static void moore_mimic(Monster *this, Coords d)
{
	if (this->state) {
		this->state = 2;
		moore_seek(this, d);
	} else if (L2(d) <= 2) {
		this->state = 1;
	}
}

static bool can_breath(Monster *this)
{
	i64 dx = abs(player.pos.x - this->pos.x);
	i64 dy = abs(player.pos.y - this->pos.y);
	if (this->class == RED_DRAGON ? dy : dx > 3 || dy >= dx || player.freeze > g.current_beat)
		return false;
	for (i8 i = 0; i < dx; ++i)
		if (BLOCKS_LOS(this->pos + i * DIRECTION(player.pos - this->pos)))
			return false;
	return true;
}

// Dragons normally chase the player cardinally every two beats (see basic_seek).
// However, as soon as the player is in breath range, they’ll charge a breath attack,
// then fire it on the next beat.
// They then resume chasing, but can’t charge another breath in the next two beats.
static void dragon(Monster *this, Coords d)
{
	if (this->aggro && this->exhausted)
		--this->exhausted;

	i8 direction = 1;

	switch (this->state) {
	case 0:
		basic_seek(this, d);
		this->state = this->delay;
		this->delay = 0;
		break;
	case 1:
		this->state = 0;
		break;
	case 2:
		direction = -1;
		// FALLTHROUGH
	case 3:
		(this->class == RED_DRAGON ? fireball : cone_of_cold)(this->pos, direction);
		this->exhausted = 3;
		this->state = 1;
		break;
	}
	if (!this->exhausted && can_breath(this))
		this->state = 2 + (d.x > 0);
}

static void elemental(Monster *this, Coords d)
{
	tile_change(this->pos, this->class == EFREET ? FIRE : ICE);
	basic_seek(this, d);
	tile_change(this->pos, this->class == EFREET ? FIRE : ICE);
}

static void mole(Monster *this, Coords d)
{
	if (this->state != (L1(d) == 1))
		this->state ^= 1;
	else
		basic_seek(this, d);
	TILE(this->pos).destroyed = true;
}

static void beetle_shed(Monster *this)
{
	tile_change(this->pos, this->class == FIRE_BEETLE ? FIRE : ICE);
	this->class = BEETLE;
}

static void beetle(Monster *this, Coords d)
{
	if (L1(d) == 1)
		beetle_shed(this);
	basic_seek(this, d);
	if (L1(player.pos - this->pos) == 1)
		beetle_shed(this);
}

static void wind_statue(__attribute__((unused)) Monster *this, Coords d)
{
	if (L1(d) == 1)
		forced_move(&player, d);
}

static void bomb_statue(Monster *this, Coords d)
{
	if (this->state)
		bomb_detonate(this, d);
	else if (L1(d) == 1)
		this->state = 1;
}

static bool can_charge(Monster *this, Coords d)
{
	if (d.x * d.y != 0 && !(this->class == ARMADILDO && abs(d.x) == abs(d.y)))
		return false;
	Coords move = DIRECTION(d);
	Coords dest = this->pos + d;
	for (Coords pos = this->pos + move; pos.x != dest.x || pos.y != dest.y; pos += move)
		if (BLOCKS_LOS(pos) || TILE(pos).monster)
			return false;
	this->prev_pos = this->pos - DIRECTION(d);
	return true;
}

// State 0: passive
// State 1: ready
// State 2: about to charge
// State 3: charging
static void armadillo(Monster *this, Coords d)
{
	this->state = this->state >= 2 ? 3 : can_charge(this, d) ? this->state + 2 : this->aggro;
	if (this->state == 3) {
		charge(this, d);
		this->delay = this->state ? 0 : 1;
	}
}

// State 0: default
// State 1: charging
static void minotaur(Monster *this, Coords d)
{
	if (this->state == 0)
		this->state = can_charge(this, d) || can_charge(this, player.prev_pos - this->pos);
	if (this->state == 1) {
		charge(this, d);
		this->delay = this->state ? 0 : 2;
	} else {
		basic_seek(this, d);
	}
}

static void digger(Monster *this, Coords d)
{
	if (this->state == 0) {
		this->state = L2(d) <= 9;
		return;
	}
	if (this->state == 1) {
		this->state = 2;
		this->delay = 2;
		return;
	}
	Coords moves[4] = {{-SIGN(d.x), 0}, {0, -SIGN(d.y)}, {0, SIGN(d.y)}, {SIGN(d.x), 0}};
	Coords move = {0, 0};
	bool vertical = abs(d.y) > (abs(d.x) + 1) / 3;
	for (i64 i = 0; i < 3; ++i) {
		move = moves[i ^ vertical];
		if (!TILE(this->pos + move).monster)
			break;
	}
	if (enemy_move(this, move) < MOVE_ATTACK) {
		this->state = 1;
		this->delay = 3;
	}
}

static void clone(Monster *this, __attribute__((unused)) Coords d)
{
	if (g.player_moved)
		enemy_move(this, DIRECTION(player.prev_pos - player.pos));
}

static void ghost(Monster *this, Coords d)
{
	this->state = (L1(d) + this->state) > L1(player.prev_pos - this->pos);
	if (this->state)
		basic_seek(this, d);
}

static void assassin(Monster *this, Coords d)
{
	this->state = L1(d) == 1 || (L1(d) + this->state) > L1(player.prev_pos - this->pos);
	(this->state ? basic_seek : basic_flee)(this, d);
}

static void sarcophagus(Monster *this, __attribute__((unused)) Coords d)
{
	static const MonsterClass types[] = {SKELETON_1, SKELETANK_1, WINDMAGE_1, RIDER_1};
	Monster *spawned = this + 1;
	this->delay = CLASS(this).beat_delay;
	if (spawned->hp > 0 || !g.seed)
		return;

	// Make sure that at least one direction isn’t blocked
	Coords spawn_dir;
	for (i64 i = 0; i < 4; ++i)
		if (IS_EMPTY(this->pos + plus_shape[i]))
			goto ok;
	return;
ok:

	do spawn_dir = plus_shape[RNG()];
		while (!IS_EMPTY(this->pos + spawn_dir));

	monster_init(spawned, types[RNG()] + this->class - SARCO_1, this->pos + spawn_dir);
	spawned->delay = 2;
	spawned->aggro = 1;
	TILE(spawned->pos).monster = (u8) (spawned - g.monsters);
}

// Move toward the player, then spawn a mummy on the empty tile closest
// to the tile immediately below (L2 distance, then bomb-order).
// ..K..
// .E@F.
// IBACJ
// .GDH.
static void mommy(Monster *this, Coords d)
{
	static const Coords spawn_dirs[] = {
		{0, 1}, {-1, 1}, {1, 1}, {0, 2}, {-1, 0}, {1, 0},
		{-1, 2}, {1, 2}, {-2, 1}, {2, 1}, {0, -1},
	};
	const Coords *spawn_dir;
	Monster *spawned = this + 1;

	bool has_moved = L1((this)->pos - (this)->prev_pos) != 0;
	basic_seek(this, d);

	if (has_moved && spawned->hp <= 0) {
		for (spawn_dir = spawn_dirs; !IS_EMPTY(this->pos + *spawn_dir); ++spawn_dir);
		monster_init(spawned, MUMMY, this->pos + *spawn_dir);
		spawned->delay = 2;
		spawned->aggro = 1;
		TILE(spawned->pos).monster = (u8) (spawned - g.monsters);
	}
}

static void ogre(Monster *this, Coords d)
{
	if (this->state == 2) {
		Coords clonk_dir = DIRECTION(player.prev_pos - this->pos);
		for (i8 i = 1; i <= 3; ++i)
			damage_tile(this->pos + i * clonk_dir, this->pos, 5, DMG_NORMAL);
		this->state = 1;
		this->delay = 2;
	} else if (d.x * d.y == 0 && abs(d.x + d.y) <= 3) {
		// Clonk!
		this->state = 2;
	} else if (this->state == 1) {
		this->state = 0;
	} else {
		basic_seek(this, d);
		this->state = 1;
	}
}

static void firepig(Monster *this, Coords d)
{
	if (d.y == 0 && abs(d.x) <= 5 && (d.x > 0) == this->state) {
		this->state += 2;
	} else if (this->state > 1) {
		fireball(this->pos, SIGN(player.pos.x - this->pos.x));
		this->state -= 2;
		this->delay = 5;
	}
}

static void devil(Monster *this, Coords d)
{
	moore_seek(this, d);
	if (this->state)
		this->delay = 0;
}

static void eye(Monster *this, Coords d)
{
	if (this->state) {
		this->state = 0;
		Coords dir = this->pos - this->prev_pos;
		this->was_requeued = true;
		enemy_move(this, dir) && enemy_move(this, dir) && enemy_move(this, dir);
	} else if (can_charge(this, d) && L1(d) <= 3) {
		this->state = 1;
	}
}

static void orc(Monster *this, Coords d)
{
	Coords old_dir = this->dir;
	this->dir = seek_dir(this, d);
	if (this->dir.x == old_dir.x && this->dir.y == old_dir.y)
		basic_seek(this, d);
	else
		this->prev_pos = this->pos;
}

static void wire_zombie(Monster *this, __attribute__((unused)) Coords d)
{
	if (enemy_move(this, this->dir) < MOVE_ATTACK && !this->requeued)
		this->dir = -this->dir;
	if (TILE(this->pos).wired && !TILE(this->pos + this->dir).wired) {
		Coords right = { -this->dir.y, this->dir.x };
		Coords left = { this->dir.y, -this->dir.x };
		if (TILE(this->pos + right).wired ^ TILE(this->pos + left).wired)
			this->dir = TILE(this->pos + right).wired ? right : left;
	}
	this->delay = IS_WIRE(this->pos) ? 0 : 1;
}

static void metrognome(Monster *this, Coords d)
{
	if (this->state) {
		basic_seek(this, d);
		--this->state;
	} else {
		mushroom(this, d);
		this->state = 3;
	}
}

const ClassInfos class_infos[256] = {
	// [Name] = { damage, max_hp, beat_delay, radius, flying, dig, priority, glyph, act }
	[GREEN_SLIME]  = { 99, 1, 0, 999, false, -1,        0, GREEN "P",  slime },
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
	[MIMIC_1]      = {  2, 1, 0,   0, false, -1, 10201100, YELLOW "m", mimic },
	[MIMIC_2]      = {  3, 1, 0,   0, false, -1, 10301100, BLUE "m",   mimic },
	[MIMIC_3]      = {  2, 1, 0,   0, false, -1, 10201100, YELLOW "m", mimic },
	[MIMIC_4]      = {  2, 1, 0,   0, false, -1, 10201100, YELLOW "m", mimic },
	[MIMIC_5]      = {  2, 1, 0,   0, false, -1, 10201100, YELLOW "m", mimic },
	[HEADLESS]     = {  1, 1, 0,   0, false, -1, 10302203, "∠",        charge },

	[SKELETANK_1]  = {  1, 1, 1,  25, false, -1, 10101202, "Z",        basic_seek },
	[SKELETANK_2]  = {  3, 2, 1,  25, false, -1, 10302204, YELLOW "Z", basic_seek },
	[SKELETANK_3]  = {  5, 3, 1,  25, false, -1, 10503206, BLACK "Z",  basic_seek },
	[WINDMAGE_1]   = {  2, 1, 1,   0, false, -1, 10201202, BLUE "@",   mage },
	[WINDMAGE_2]   = {  4, 2, 1,   0, false, -1, 10402204, YELLOW "@", mage },
	[WINDMAGE_3]   = {  5, 3, 1,   0, false, -1, 10503206, BLACK "@",  mage },
	[MUSHROOM_1]   = {  2, 1, 3,  25, false, -1, 10201402, BLUE "%",   mushroom },
	[MUSHROOM_2]   = {  4, 3, 2,  25, false, -1, 10403303, PURPLE "%", mushroom },
	[GOLEM_1]      = {  4, 5, 3,  25,  true,  2, 20405404, "'",        basic_seek },
	[GOLEM_2]      = {  6, 7, 3,  25,  true,  2, 20607407, BLACK "'",  basic_seek },
	[ARMADILLO_1]  = {  2, 1, 0, 999, false,  2, 10201102, "q",        armadillo },
	[ARMADILLO_2]  = {  3, 2, 0, 999, false,  2, 10302105, YELLOW "q", armadillo },
	[CLONE]        = {  3, 1, 0,  25, false, -1, 10301102, "@",        clone },
	[TARMONSTER]   = {  3, 1, 0,   0, false, -1, 10304103, "t",        mimic },
	[MOLE]         = {  2, 1, 0,  25,  true, -1,  1020113, "r",        mole },
	[WIGHT]        = {  2, 1, 0,   9,  true, -1, 10201103, GREEN "W",  basic_seek },
	[WALL_MIMIC]   = {  2, 1, 0,   0, false, -1, 10201103, GREEN "m",  mimic },
	[LIGHTSHROOM]  = {  0, 1, 0,   0, false, -1,        0, "%",        NULL },
	[BOMBSHROOM]   = {  4, 1, 0,   0, false, -1,      ~2u, YELLOW "%", NULL },
	[BOMBSHROOM_]  = {  4, 1, 0,   9, false, -1,      ~2u, RED "%",    bomb_detonate },

	[FIRE_SLIME]   = {  3, 1, 0, 999, false,  2, 10301101, RED "P",    slime },
	[ICE_SLIME]    = {  3, 1, 0, 999, false,  2, 10301101, CYAN "P",   slime },
	[RIDER_1]      = {  2, 1, 0,  49,  true, -1, 10201102, "&",        basic_seek },
	[RIDER_2]      = {  4, 2, 0,  49,  true, -1, 10402104, YELLOW "&", basic_seek },
	[RIDER_3]      = {  6, 3, 0,  49,  true, -1, 10603106, BLACK "&",  basic_seek },
	[EFREET]       = {  3, 2, 2,  49,  true,  2, 20302302, RED "E",    elemental },
	[DJINN]        = {  3, 2, 2,  49,  true,  2, 20302302, CYAN "E",   elemental },
	[ASSASSIN_1]   = {  4, 1, 0,  49, false, -1, 10401103, PURPLE "o", assassin },
	[ASSASSIN_2]   = {  6, 2, 0,  49, false, -1, 10602105, BLACK "o",  assassin },
	[FIRE_BEETLE]  = {  3, 3, 1,  49, false, -1, 10303202, RED "a",    beetle },
	[ICE_BEETLE]   = {  3, 3, 1,  49, false, -1, 10303202, CYAN "a",   beetle },
	[BEETLE]       = {  3, 3, 1,  49, false, -1, 10303202, "a",        basic_seek },
	[HELLHOUND]    = {  3, 1, 1,   9, false, -1, 10301202, RED "d",    moore_seek },
	[SHOVE_1]      = {  0, 2, 0,  49, false, -1, 10002102, PURPLE "~", basic_seek },
	[SHOVE_2]      = {  0, 3, 0,  49, false, -1, 10003102, BLACK "~",  basic_seek },
	[YETI]         = {  3, 1, 3,  49,  true,  2, 20301403, CYAN "Y",   yeti },
	[GHAST]        = {  2, 1, 0,   9,  true, -1, 10201102, PURPLE "W", basic_seek },
	[FIRE_MIMIC]   = {  2, 1, 0,   0, false, -1, 10201102, RED "m",    mimic },
	[ICE_MIMIC]    = {  2, 1, 0,   0, false, -1, 10201102, CYAN "m",   mimic },
	[FIRE_POT]     = {  0, 1, 0,   0, false, -1,        0, RED "(",    NULL },
	[ICE_POT]      = {  0, 1, 0,   0, false, -1,        0, CYAN "(",   NULL },

	[BOMBER]       = {  4, 1, 1,   0, false, -1, 99999998, RED "o",    diagonal_seek },
	[DIGGER]       = {  1, 1, 1,   9, false,  2, 10101201, "o",        digger },
	[BLACK_BAT]    = {  2, 1, 0,   9,  true, -1, 10401120, BLACK "B",  black_bat },
	[ARMADILDO]    = {  3, 3, 0, 999, false,  2, 10303104, ORANGE "q", armadillo },
	[BLADENOVICE]  = {  1, 1, 1,   9, false, -1, 99999995, "b",        blademaster },
	[BLADEMASTER]  = {  2, 2, 1,   9, false, -1, 99999996, YELLOW "b", blademaster },
	[GHOUL]        = {  1, 1, 0,   9,  true, -1, 10301102, "W",        moore_seek },
	[GOOLEM]       = {  5, 5, 3,   9,  true,  2, 20510407, GREEN "'",  basic_seek },
	[HARPY]        = {  3, 1, 1,   0,  true, -1, 10301203, GREEN "h",  harpy },
	[LICH_1]       = {  2, 1, 1,   0, false, -1, 10404202, GREEN "L",  mage },
	[LICH_2]       = {  3, 2, 1,   0, false, -1, 10404302, PURPLE "L", mage },
	[LICH_3]       = {  4, 3, 1,   0, false, -1, 10404402, BLACK "L",  mage },
	[CONF_MONKEY]  = {  0, 1, 0,   9, false, -1, 10004103, GREEN "Y",  basic_seek },
	[TELE_MONKEY]  = {  0, 2, 0,   9, false, -1, 10002103, PINK "Y",   basic_seek },
	[PIXIE]        = {  4, 1, 0,   9,  true, -1, 10401102, "n",        basic_seek },
	[SARCO_1]      = {  0, 1, 7,   9, false, -1, 10101805, "|",        sarcophagus },
	[SARCO_2]      = {  0, 2, 9,   9, false, -1, 10102910, YELLOW "|", sarcophagus },
	[SARCO_3]      = {  0, 3, 11,  9, false, -1, 10103915, BLACK "|",  sarcophagus },
	[SPIDER]       = {  2, 1, 1,   9, false, -1, 10401202, YELLOW "s", basic_seek },
	[FREE_SPIDER]  = {  2, 1, 0,   9, false, -1, 10401202, RED "s",    diagonal_seek },
	[WARLOCK_1]    = {  3, 1, 1,   9, false, -1, 10401202, "w",        basic_seek },
	[WARLOCK_2]    = {  4, 2, 1,   9, false, -1, 10401302, YELLOW "w", basic_seek },
	[MUMMY]        = {  2, 1, 1,   9, false, -1, 30201103, "M",        moore_seek },
	[WIND_STATUE]  = {  4, 1, 0,   0, false, -1, 10401102, CYAN "g",   wind_statue },
	[MIMIC_STATUE] = {  4, 1, 0,   0, false, -1, 10401102, BLACK "g",  mimic },
	[BOMB_STATUE]  = {  4, 1, 0,   0, false, -1, 10401102, YELLOW "g", bomb_statue },
	[MINE_STATUE]  = {  4, 1, 0,   0, false, -1,        0, RED "g",    NULL },
	[CRATE_1]      = {  0, 1, 0,   0, false, -1,        0, "(",        NULL },
	[CRATE_2]      = {  0, 1, 0,   0, false, -1,        0, "(",        NULL },
	[BARREL]       = {  1, 1, 0, 999, false,  1,       80, YELLOW "(", charge },
	[TEH_URN]      = {  0, 3, 0,   0, false, -1,        0, PURPLE "(", NULL },
	[CHEST]        = {  0, 1, 0,   0, false, -1,        0, BLACK "(",  NULL },
	[FIREPIG]      = {  5, 1, 0,   0, false, -1,        1, RED "q",    firepig },

	[SKULL_1]      = {  1, 1, 1,   9, false, -1, 10101200, WHITE "z",  basic_seek },
	[SKULL_2]      = {  2, 1, 1,   9, false, -1, 10202200, YELLOW "z", basic_seek },
	[SKULL_3]      = {  4, 1, 1,   9, false, -1, 10403200, BLACK "z",  basic_seek },
	[WATER_BALL]   = {  0, 1, 0,   9,  true, -1, 10001101, BLUE "e",   moore_seek },
	[TAR_BALL]     = {  0, 1, 0,   9,  true, -1, 10001102, BLACK "e",  moore_seek },
	[ELECTRO_1]    = {  1, 1, 1,   0, false, -1, 10101202, BLUE "L",   NULL },
	[ELECTRO_2]    = {  3, 2, 1,   0, false, -1, 10302203, RED "L",    NULL },
	[ELECTRO_3]    = {  4, 3, 1,   0, false, -1, 10403204, YELLOW "L", NULL },
	[ORB_1]        = {  1, 1, 0,   0,  true,  1, 10101100, YELLOW "e", charge },
	[ORB_2]        = {  3, 1, 0,   0,  true,  1, 10301100, YELLOW "e", charge },
	[ORB_3]        = {  4, 1, 0,   0,  true,  1, 10401100, YELLOW "e", charge },
	[GORGON_1]     = {  0, 1, 0,   9, false, -1, 10001102, GREEN "S",  basic_seek },
	[GORGON_2]     = {  0, 3, 0,   9, false, -1, 10003100, YELLOW "S", basic_seek },
	[WIRE_ZOMBIE]  = {  2, 1, 0, 999, false, -1, 10201201, ORANGE "Z", wire_zombie },
	[EYE_1]        = {  1, 1, 0,   0,  true,  2, 10101103, GREEN "e",  eye },
	[EYE_2]        = {  2, 2, 0,   0,  true,  2, 10202105, PINK "e",   eye },
	[ORC_1]        = {  1, 1, 0,   9, false, -1, 10101102, GREEN "o",  orc },
	[ORC_2]        = {  2, 2, 0,   9, false, -1, 10202103, PINK "o",   orc },
	[ORC_3]        = {  3, 3, 0,   9, false, -1, 10303104, PURPLE "o", orc },
	[DEVIL_1]      = {  2, 1, 2,   9, false, -1, 10201303, RED "&",    devil },
	[DEVIL_2]      = {  4, 2, 2,   9, false, -1, 10402305, GREEN "&",  devil },
	[PURPLE_SLIME] = {  3, 1, 0, 999, false, -1, 10301102, PURPLE "P", slime },
	[CURSE]        = {  0, 1, 0,   9,  true, -1, 10001102, YELLOW "W", basic_seek },
	[SHOP_MIMIC]   = {  2, 1, 0,   0, false, -1, 10201100, YELLOW "m", moore_mimic },

	[DIREBAT_1]    = {  3, 2, 1,   9,  true, -1, 30302210, YELLOW "B", bat },
	[DIREBAT_2]    = {  4, 3, 1,   9,  true, -1, 30403215, "B",        bat },
	[DRAGON]       = {  4, 4, 1,  49,  true,  4, 30404210, GREEN "D",  basic_seek },
	[RED_DRAGON]   = {  6, 6, 1, 100,  true,  4, 99999999, RED "D",    dragon },
	[BLUE_DRAGON]  = {  6, 6, 1,   0,  true,  4, 99999997, BLUE "D",   dragon },
	[EARTH_DRAGON] = {  6, 6, 1,  49,  true,  4, 30606215, BROWN "D",  basic_seek },
	[BANSHEE_1]    = {  4, 3, 0,  25,  true, -1, 30403110, BLUE "8",   basic_seek },
	[BANSHEE_2]    = {  6, 4, 0,   9,  true, -1, 30604115, GREEN "8",  basic_seek },
	[MINOTAUR_1]   = {  4, 3, 0,  49,  true,  2, 30403110, "H",        minotaur },
	[MINOTAUR_2]   = {  5, 5, 0,  49,  true,  2, 30505115, BLACK "H",  minotaur },
	[NIGHTMARE_1]  = {  4, 3, 1,  81,  true,  4, 30403210, BLACK "u",  basic_seek },
	[NIGHTMARE_2]  = {  5, 5, 1,  81,  true,  4, 30505215, RED "u",    basic_seek },
	[MOMMY]        = {  4, 6, 3,   9,  true, -1, 30405215, BLACK "@",  mommy },
	[OGRE]         = {  5, 5, 2,   9,  true,  2, 30505115, GREEN "O",  ogre },
	[METROGNOME_1] = {  4, 3, 0,   9,  true,  1, 30403115, YELLOW "G", metrognome },
	[METROGNOME_2] = {  5, 5, 0,   9,  true,  1, 30505115, GREEN "G",  metrognome },

	[SHOPKEEPER]   = { 20, 9, 0,   0, false, -1, 99999997, "@",        NULL },
	[PLAYER]       = {  1, 1, 0,   0, false, -1,      ~0u, "@",        NULL },
	[BOMB]         = {  4, 0, 0,   0, false, -1,      ~1u, "x",        bomb_detonate },
};
