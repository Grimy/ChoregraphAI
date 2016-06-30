// monsters.c - defines all monsters in the game and their AIs

#define MOVE(x, y) (enemy_move(this, (Coords) {(x), (y)}))

// Many things in the game follow the so-called “bomb order”:
// 147
// 258
// 369
// Most monster AIs use this as a tiebreaker: when several destination tiles fit
// all criteria equally well, monsters pick the one that comes first in bomb-order.

// Move cardinally toward the player, avoiding obstacles.
// Try to keep moving along the same axis, unless the monster’s current or
// previous position is aligned with the player’s current or previous position.
static void basic_seek(Monster *this, Coords d)
{
	Coords vertical = {0, SIGN(d.y)};
	Coords horizontal = {SIGN(d.x), 0};

	// Ignore the player’s previous position if they moved more than one tile
	Coords prev_pos = L1(player.pos - player.prev_pos) > 1 ? player.pos : player.prev_pos;

	this->vertical =
		// #1: move toward the player
		d.y == 0 ? 0 :
		d.x == 0 ? 1 :

		// #2: avoid obstacles
		!can_move(this, vertical) ? 0 :
		!can_move(this, horizontal) ? 1 :

		// #3: move toward the player’s previous position
		this->pos.y == prev_pos.y ? 0 :
		this->pos.x == prev_pos.x ? 1 :

		// #4: weird edge cases
		this->prev_pos.y == player.pos.y ? d.x == 1 :
		this->prev_pos.x == player.pos.x ? !(d.x < 0 && ABS(d.y) == 1) :
		this->prev_pos.y == prev_pos.y ? ABS(d.x) == 1 || (d.x == 2 && player.pos.x > spawn.x) :
		this->prev_pos.x == prev_pos.x ? ABS(d.y) > 1 + (d.x < 0) :

		// #5: keep moving along the same axis
		this->vertical;

	enemy_move(this, this->vertical ? vertical : horizontal);
}

// Move away from the player.
// Tiebreak by L2 distance, then bomb-order.
static void basic_flee(Monster *this, Coords d)
{
	if (d.y == 0)
		MOVE(-SIGN(d.x), 0) || MOVE(0, -1) || MOVE(0, 1);
	else if (d.x == 0)
		MOVE(0, -SIGN(d.y)) || MOVE(-1, 0) || MOVE(1, 0);
	else if (ABS(d.y) > ABS(d.x))
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
	if (MOVE(SIGN(d.x), SIGN(d.y)))
		return;
	if (d.x < 0)
		MOVE(-1, 0) || MOVE(0, SIGN(d.y));
	else
		MOVE(0, SIGN(d.y)) || MOVE(1, 0);
}

// Move in a random direction.
// If the chosen direction is blocked, cycles through the other directions
// in the order right > left > down > up (mnemonic: Ryan Loves to Dunk Us).
static void bat(Monster *this, Coords d)
{
	static const Coords moves[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	if (this->confusion) {
		basic_seek(this, d);
		return;
	}
	if (!seed) {
		monster_kill(this, DMG_NORMAL);
		return;
	}
	i64 rng = RNG();
	for (i64 i = 0; i < 4; ++i)
		if (enemy_move(this, moves[(rng + i) & 3]))
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
	if (this->state == 0) {
		basic_seek(this, d);
	} else if (this->state == 1) {
		enemy_move(this, DIRECTION(player.prev_pos - this->prev_pos));
		enemy_move(this, DIRECTION(player.prev_pos - this->prev_pos));
		this->state = 2;
		this->delay = 0;
	} else if (this->state == 2) {
		this->state = 0;
	}
}

// Common AI for liches and windmages.
static void mage(Monster *this, Coords d)
{
	bool is_lich = this->class >= LICH_1 && this->class <= LICH_3;
	bool can_cast = L2(d) == 4
		&& can_move(this, DIRECTION(d))
		&& !this->confusion
		&& TILE(this->pos).class != WATER
		&& (TILE(this->pos).class != TAR || this->untrapped)
		&& !(player.confusion && is_lich);

	if (!can_cast) {
		basic_seek(this, d);
		return;
	}

	this->delay = 1;
	if (is_lich)
		player.confusion = 5;
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
	basic_seek(this, d);
	if ((this->pos.x != this->prev_pos.x || this->pos.y != this->prev_pos.y)
	    && L2(player.pos - this->pos) < 4)
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
	for (i64 i = 0; i < LENGTH(moves); ++i) {
		Coords move = moves[i];
		if ((L2(move) == 9 || L2(move) == 4)
		    && (BLOCKS_MOVEMENT(this->pos + DIRECTION(move))
		    || BLOCKS_MOVEMENT(this->pos + 2*DIRECTION(move))))
			continue;
		if (L2(move) == 5
		    && BLOCKS_MOVEMENT(this->pos + move / 2)
		    && (BLOCKS_MOVEMENT(this->pos + DIRECTION(move))
		    || BLOCKS_MOVEMENT(this->pos + DIRECTION(move) - move / 2)))
			continue;
		i64 score = L1(d - move);
		if (score && score < min && can_move(this, move)) {
			min = score;
			best_move = move;
		}
	}
	enemy_move(this, best_move);
}

static void zombie(Monster *this, __attribute__((unused)) Coords d)
{
	static const Coords moves[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	this->state ^= enemy_move(this, moves[this->state]) < MOVE_ATTACK;
	this->delay = 1;
}

static void blue_slime(Monster *this, __attribute__((unused)) Coords d)
{
	static const Coords moves[] = {{0, -1}, {0, 1}, {0, -1}, {0, 1}};
	this->state += enemy_move(this, moves[this->state]) == MOVE_SUCCESS;
}

static void yellow_slime(Monster *this, __attribute__((unused)) Coords d)
{
	static const Coords moves[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
	this->state += enemy_move(this, moves[this->state]) == MOVE_SUCCESS;
}

static void diagonal_slime(Monster *this, __attribute__((unused)) Coords d)
{
	static const Coords moves[] = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
	this->state += enemy_move(this, moves[this->state]) == MOVE_SUCCESS;
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

static bool can_breath(Monster *this)
{
	i64 dx = ABS(player.pos.x - this->pos.x);
	i64 dy = ABS(player.pos.y - this->pos.y);
	return this->class == RED_DRAGON ? !dy : dx < 4 && dy < dx && !player.freeze;
}

static void breath_attack(Monster *this)
{
	i8 direction = SIGN(player.pos.x - this->pos.x);
	(this->class == RED_DRAGON ? fireball : cone_of_cold)(this->pos, direction);
}

// Dragons normally chase the player cardinally every two beats (see basic_seek).
// However, as soon as the player is in breath range, they’ll charge a breath attack,
// then fire it on the next beat.
// They then resume chasing, but can’t charge another breath in the next two beats.
static void dragon(Monster *this, Coords d)
{
	g.dragon_exhausted -= this->aggro && g.dragon_exhausted;
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
		breath_attack(this);
		g.dragon_exhausted = 3;
		this->state = 1;
		break;
	}
	if (!g.dragon_exhausted && can_breath(this) && can_see(this->pos))
		this->state = 2;
}

static void elemental(Monster *this, Coords d)
{
	tile_change(&TILE(this->pos), this->class == EFREET ? FIRE : ICE);
	basic_seek(this, d);
	tile_change(&TILE(this->pos), this->class == EFREET ? FIRE : ICE);
}

static void mole(Monster *this, Coords d)
{
	if (this->state != (L1(d) == 1))
		this->state ^= 1;
	else
		basic_seek(this, d);
	TILE(this->pos).traps_destroyed = true;
}

static void beetle_shed(Monster *this)
{
	tile_change(&TILE(this->pos), this->class == FIRE_BEETLE ? FIRE : ICE);
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
	if (d.x * d.y != 0 && (ABS(d.x) != ABS(d.y) || this->class != ARMADILDO))
		return false;
	Coords move = DIRECTION(d);
	Coords dest = this->pos + d;
	for (Coords pos = this->pos + move; pos.x != dest.x || pos.y != dest.y; pos += move)
		if (TILE(pos).class == WALL || TILE(pos).monster)
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
	i64 old_state = this->state;
	this->state = this->state >= 2 ? 3 : can_charge(this, d) ? this->state + 2 : this->aggro;
	if (this->state == 3) {
		Coords charging_dir = this->pos - this->prev_pos;
		if (this->class != ARMADILDO)
			charging_dir = CARDINALIZE(charging_dir);
		if (enemy_move(this, this->pos - this->prev_pos) != MOVE_SUCCESS) {
			this->delay = 1;
			this->state = 0;
		}
	}
	if (old_state && this->confusion)
		this->prev_pos = this->pos + this->pos - this->prev_pos;
}

// State 0: default
// State 1: charging
static void minotaur(Monster *this, Coords d)
{
	if (this->state == 0) {
		this->state = can_charge(this, d) || can_charge(this, player.prev_pos - this->pos);
	}
	if (this->state == 1) {
		if (!enemy_move(this, this->pos - this->prev_pos)) {
			this->delay = 2;
			this->state = 0;
		}
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
	this->vertical = d.y > (d.x + 1) / 3;
	for (i64 i = 0; i < 3; ++i) {
		move = moves[i ^ this->vertical];
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

static void headless(Monster *this, __attribute__((unused)) Coords d)
{
	Coords prev_pos = this->prev_pos;
	if (!enemy_move(this, CARDINALIZE(this->pos - prev_pos)))
		this->prev_pos = prev_pos;
}

static void sarcophagus(Monster *this, __attribute__((unused)) Coords d) {
	this->delay = CLASS(this).beat_delay;
	if (!g.sarco_on || !seed || this[1].hp > 0)
		return;

	// Make sure that at least one direction isn’t blocked
	Coords dir;
	for (i64 i = 0; i < 4; ++i)
		if (can_move(this, plus_shape[i]))
			goto ok;
	return;
ok:

	do dir = plus_shape[RNG()];
		while (!can_move(this, dir));

	this[1].class = (MonsterClass[]) {SKELETON_1, SKELETANK_1, WINDMAGE_1, RIDER_1} [RNG()];
	this[1].class += this->class - SARCO_1;
	this[1].pos = this->pos + dir;
	this[1].delay = 1;
	TILE(this[1].pos).monster = &this[1];
}

static void ogre(Monster *this, Coords d) {
	if (this->state == 2) {
		Coords clonk_dir = DIRECTION(player.prev_pos - this->pos);
		for (i8 i = 1; i <= 3; ++i)
			damage_tile(this->pos + i * clonk_dir, this->pos, 5, DMG_NORMAL);
		this->state = 1;
		this->delay = 2;
	} else if (d.x * d.y == 0 && ABS(d.x + d.y) <= 3) {
		// Clonk!
		this->state = 2;
	} else if (this->state == 1) {
		this->state = 0;
	} else {
		basic_seek(this, d);
		this->state = 1;
	}
}

static void firepig(Monster *this, Coords d) {
	if (d.y == 0 && ABS(d.x) <= 5 && (d.x > 0) == this->state) {
		this->state += 2;
	} else if (this->state > 1) {
		fireball(this->pos, SIGN(player.pos.x - this->pos.x));
		this->state -= 2;
		this->delay = 5;
	}
}

static void nop() {}

static const ClassInfos class_infos[256] = {
	// [Name] = { max_hp, beat_delay, radius, flying, dig, priority, glyph, act }
	[GREEN_SLIME] = { 1, 9, 225, false, -1, 19901101, GREEN "P",  nop },
	[BLUE_SLIME]  = { 2, 1, 225, false, -1, 10202202, BLUE "P",   blue_slime },
	[YOLO_SLIME]  = { 1, 0, 225, false, -1, 10101102, YELLOW "P", yellow_slime },
	[SKELETON_1]  = { 1, 1,   9, false, -1, 10101202, "Z",        basic_seek },
	[SKELETON_2]  = { 2, 1,   9, false, -1, 10302203, YELLOW "Z", basic_seek },
	[SKELETON_3]  = { 3, 1,   9, false, -1, 10403204, BLACK "Z",  basic_seek },
	[BLUE_BAT]    = { 1, 1,   9,  true, -1, 10101202, BLUE "B",   bat },
	[RED_BAT]     = { 1, 0,   9,  true, -1, 10201103, RED "B",    bat },
	[GREEN_BAT]   = { 1, 0,   9,  true, -1, 10301120, GREEN "B",  bat },
	[MONKEY_1]    = { 1, 0,   9, false, -1, 10004101, PURPLE "Y", basic_seek },
	[MONKEY_2]    = { 2, 0,   9, false, -1, 10006103, "Y",        basic_seek },
	[GHOST]       = { 1, 0,   9,  true, -1, 10201102, "8",        ghost },
	[ZOMBIE]      = { 1, 1, 225, false, -1, 10201201, GREEN "Z",  zombie },
	[WRAITH]      = { 1, 0,   9,  true, -1, 10101102, RED "W",    basic_seek },
	[MIMIC_1]     = { 1, 0,   0, false, -1, 10201100, YELLOW "m", mimic },
	[MIMIC_2]     = { 1, 0,   0, false, -1, 10301100, BLUE "m",   mimic },
	[HEADLESS]    = { 1, 0,   0, false, -1, 10302203, "∠",        headless },

	[SKELETANK_1] = { 1, 1,   9, false, -1, 10101202, "Z",        basic_seek },
	[SKELETANK_2] = { 2, 1,   9, false, -1, 10302204, YELLOW "Z", basic_seek },
	[SKELETANK_3] = { 3, 1,   9, false, -1, 10503206, BLACK "Z",  basic_seek },
	[WINDMAGE_1]  = { 1, 1,   0, false, -1, 10201202, BLUE "@",   mage },
	[WINDMAGE_2]  = { 2, 1,   0, false, -1, 10402204, YELLOW "@", mage },
	[WINDMAGE_3]  = { 3, 1,   0, false, -1, 10503206, BLACK "@",  mage },
	[MUSHROOM_1]  = { 1, 3,   9, false, -1, 10201402, BLUE "%",   mushroom },
	[MUSHROOM_2]  = { 3, 2,   9, false, -1, 10403303, PURPLE "%", mushroom },
	[GOLEM_1]     = { 5, 3,   9,  true,  2, 20405404, "'",        basic_seek },
	[GOLEM_2]     = { 7, 3,   9,  true,  2, 20607407, BLACK "'",  basic_seek },
	[ARMADILLO_1] = { 1, 0, 225, false,  2, 10201102, "q",        armadillo },
	[ARMADILLO_2] = { 2, 0, 225, false,  2, 10302105, YELLOW "q", armadillo },
	[CLONE]       = { 1, 0,   9, false, -1, 10301102, "@",        clone },
	[TARMONSTER]  = { 1, 0,   9, false, -1, 10304103, "t",        mimic },
	[MOLE]        = { 1, 0,   9,  true, -1,  1020113, "r",        mole },
	[WIGHT]       = { 1, 0,   9,  true, -1, 10201103, GREEN "W",  basic_seek },
	[WALL_MIMIC]  = { 1, 0,   0, false, -1, 10201103, GREEN "m",  mimic },
	[LIGHTSHROOM] = { 1, 9,   9, false, -1,        0, "%",        nop },
	[BOMBSHROOM]  = { 1, 0,   0, false, -1,      ~1u, YELLOW "%", nop },
	[BOMBSHROOM_] = { 1, 0,   9, false, -1,      ~1u, RED "%",    bomb_detonate },

	[FIRE_SLIME]  = { 1, 0, 225, false,  2, 10301101, RED "P",    diagonal_slime },
	[ICE_SLIME]   = { 1, 0, 225, false,  2, 10301101, CYAN "P",   diagonal_slime },
	[RIDER_1]     = { 1, 0,   9,  true, -1, 10201102, "&",        basic_seek },
	[RIDER_2]     = { 2, 0,   9,  true, -1, 10402104, YELLOW "&", basic_seek },
	[RIDER_3]     = { 3, 0,   9,  true, -1, 10603106, BLACK "&",  basic_seek },
	[EFREET]      = { 2, 2,   9,  true,  2, 20302302, RED "E",    elemental },
	[DJINN]       = { 2, 2,   9,  true,  2, 20302302, CYAN "E",   elemental },
	[ASSASSIN_1]  = { 1, 0,   9, false, -1, 10401103, PURPLE "G", assassin },
	[ASSASSIN_2]  = { 2, 0,   9, false, -1, 10602105, BLACK "G",  assassin },
	[FIRE_BEETLE] = { 3, 1,  49, false, -1, 10303202, RED "a",    beetle },
	[ICE_BEETLE]  = { 3, 1,  49, false, -1, 10303202, CYAN "a",   beetle },
	[BEETLE]      = { 3, 1,  49, false, -1, 10303202, "a",        basic_seek },
	[HELLHOUND]   = { 1, 1,   9, false, -1, 10301202, RED "d",    moore_seek },
	[SHOVE_1]     = { 2, 0,   9, false, -1, 10002102, PURPLE "~", basic_seek },
	[SHOVE_2]     = { 3, 0,   9, false, -1, 10003102, BLACK "~",  basic_seek },
	[YETI]        = { 1, 3,   9,  true,  2, 20301403, CYAN "Y",   yeti },
	[GHAST]       = { 1, 0,   9,  true, -1, 10201102, PURPLE "W", basic_seek },
	[FIRE_MIMIC]  = { 1, 0,   0, false, -1, 10201102, RED "m",    mimic },
	[ICE_MIMIC]   = { 1, 0,   0, false, -1, 10201102, CYAN "m",   mimic },
	[FIRE_POT]    = { 1, 9,   9, false, -1,        0, RED "(",    nop },
	[ICE_POT]     = { 1, 0,   9, false, -1,        0, CYAN "(",   nop },

	[BOMBER]      = { 1, 1,   0, false, -1, 99999998, RED "G",    diagonal_seek },
	[DIGGER]      = { 1, 1,   9, false,  2, 10101201, "G",        digger },
	[BLACK_BAT]   = { 1, 0,   9,  true, -1, 10401120, BLACK "B",  black_bat },
	[ARMADILDO]   = { 3, 0, 225, false,  2, 10303104, ORANGE "q", armadillo },
	[BLADENOVICE] = { 1, 1,   9, false, -1, 99999995, "b",        blademaster },
	[BLADEMASTER] = { 2, 1,   9, false, -1, 99999996, "b",        blademaster },
	[GHOUL]       = { 1, 0,   9,  true, -1, 10301102, "W",        moore_seek },
	[GOOLEM]      = { 5, 3,   9,  true,  2, 20510407, GREEN "'",  basic_seek },
	[HARPY]       = { 1, 1,   0,  true, -1, 10301203, GREEN "h",  harpy },
	[LICH_1]      = { 1, 1,   0, false, -1, 10404202, GREEN "L",  mage },
	[LICH_2]      = { 2, 1,   0, false, -1, 10404302, PURPLE "L", mage },
	[LICH_3]      = { 3, 1,   0, false, -1, 10404402, BLACK "L",  mage },
	[CONF_MONKEY] = { 1, 0,   9, false, -1, 10004103, GREEN "Y",  basic_seek },
	[TELE_MONKEY] = { 2, 0,   9, false, -1, 10002103, PINK "Y",   basic_seek },
	[PIXIE]       = { 1, 0,   9,  true, -1, 10401102, "n",        basic_seek },
	[SARCO_1]     = { 1, 7,   9, false, -1, 10101805, "|",        sarcophagus },
	[SARCO_2]     = { 2, 9,   9, false, -1, 10102910, YELLOW "|", sarcophagus },
	[SARCO_3]     = { 3, 11,  9, false, -1, 10103915, BLACK "|",  sarcophagus },
	[SPIDER]      = { 1, 1,   9, false, -1, 10401202, YELLOW "s", basic_seek },
	[FREE_SPIDER] = { 1, 0,   9, false, -1, 10401202, YELLOW "s", diagonal_seek },
	[WARLOCK_1]   = { 1, 1,   9, false, -1, 10401202, "w",        basic_seek },
	[WARLOCK_2]   = { 2, 1,   9, false, -1, 10401302, YELLOW "w", basic_seek },
	[MUMMY]       = { 1, 1,   9, false, -1, 30201103, "M",        moore_seek },
	[WIND_STATUE] = { 1, 0,   0, false, -1, 10401102, CYAN "g",   wind_statue },
	[SEEK_STATUE] = { 1, 0,   0, false, -1, 10401102, BLACK "g",  mimic },
	[BOMB_STATUE] = { 1, 0,   0, false, -1, 10401102, YELLOW "g", bomb_statue },
	[MINE_STATUE] = { 1, 0,   0, false, -1, 10401102, RED "g",    nop },
	[CRATE_1]     = { 1, 1,   0, false, -1, 10401102, "(",        nop },
	[CRATE_2]     = { 1, 1,   0, false, -1, 10401102, "g",        nop },
	[FIREPIG]     = { 1, 0,   0, false, -1,        1, RED "q",    firepig },

	[SHOPKEEPER]  = { 9, 9,   9, false, -1, 99999997, "@",        nop },
	[DIREBAT_1]   = { 2, 1,   9,  true, -1, 30302210, YELLOW "B", bat },
	[DIREBAT_2]   = { 3, 1,   9,  true, -1, 30403215, "B",        bat },
	[DRAGON]      = { 4, 1,  49,  true,  4, 30404210, GREEN "D",  basic_seek },
	[RED_DRAGON]  = { 6, 1, 225,  true,  4, 99999999, RED "D",    dragon },
	[BLUE_DRAGON] = { 6, 1,   0,  true,  4, 99999997, BLUE "D",   dragon },
	[BANSHEE_1]   = { 3, 0,  49,  true, -1, 30403110, BLUE "8",   basic_seek },
	[BANSHEE_2]   = { 4, 0,  49,  true, -1, 30604115, GREEN "8",  basic_seek },
	[MINOTAUR_1]  = { 3, 0,  49,  true,  2, 30403110, "H",        minotaur },
	[MINOTAUR_2]  = { 5, 0,  49,  true,  2, 30505115, BLACK "H",  minotaur },
	[NIGHTMARE_1] = { 3, 1,  81,  true,  4, 30403210, BLACK "u",  basic_seek },
	[NIGHTMARE_2] = { 5, 1,  81,  true,  4, 30505215, RED "u",    basic_seek },
	[MOMMY]       = { 6, 3,  49,  true, -1, 30405215, BLACK "@",  basic_seek },
	[OGRE]        = { 5, 2,   9,  true,  2, 30505115, GREEN "O",  ogre },

	[PLAYER]      = { 1, 0,   0, false, -1,      ~0u, "@",        NULL },
	[BOMB]        = { 0, 0,   0, false, -1,      ~1u, "o",        bomb_detonate },
};
