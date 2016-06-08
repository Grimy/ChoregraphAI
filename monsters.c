// monsters.c - defines all monsters in the game and their AIs

#define MOVE(x, y) (enemy_move(this, (Coords) {(x), (y)}))

// Many things in the game follow the so-called “bomb order”:
// 147
// 258
// 369
// Most monster AIs use this as a tiebreaker: when several destination tiles fit
// all criteria equally well, monsters pick the one that comes first in bomb-order.

// Move cardinally toward the player.
// Try to keep moving along the same axis, unless the monster’s current or
// previous position is aligned with the player’s current or previous position.
static void basic_seek(Monster *this, Coords d) {
	Coords vertical = {0, SIGN(d.y)};
	Coords horizontal = {SIGN(d.x), 0};

	this->vertical =
		// #1: move toward the player
		d.y == 0 ? 0 :
		d.x == 0 ? 1 :

		// #2: avoid obstacles (when both axes are blocked, tiebreak by L2 distance)
		!can_move(this, vertical) ? !can_move(this, horizontal) && ABS(d.y) > ABS(d.x) :
		!can_move(this, horizontal) ? 1 :

		// #3: if pos aligns with the player’s prevpos or vice-versa, switch axes
		this->pos.y == player.prev_pos.y || this->prev_pos.y == player.pos.y ? 0 :
		this->pos.x == player.prev_pos.x || this->prev_pos.x == player.pos.x ? 1 :

		// #4: if prevpos aligns with the player’s prevpos, tiebreak by L2 distance
		this->prev_pos.y == player.prev_pos.y ? ABS(d.y) > ABS(d.x) :
		this->prev_pos.x == player.prev_pos.x ? ABS(d.y) > ABS(d.x) :

		// #6: keep moving along the same axis
		this->vertical;

	enemy_move(this, this->vertical ? vertical : horizontal);
}

// Move diagonally toward the player. The tiebreaker is *reverse* bomb-order.
static void diagonal_seek(Monster *this, Coords d) {
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
static void moore_seek(Monster *this, Coords d) {
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
static void bat(Monster *this, Coords d) {
	static const Coords moves[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	if (this->confusion) {
		basic_seek(this, d);
		return;
	}
	long rng = rand();
	for (long i = 0; i < 4; ++i)
		if (enemy_move(this, moves[(rng + i) & 3]))
			return;
}

// Attack the player if possible, otherwise move randomly.
static void black_bat(Monster *this, Coords d) {
	if (L1(d) == 1)
		enemy_move(this, d);
	else
		bat(this, d);
}

// basic_seek variant used by blademasters.
// After parrying a melee hit, lunge two tiles in the direction the hit came from.
static void blademaster(Monster *this, Coords d) {
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

static bool can_cast(Monster *this, Coords d) {
	return L2(d) == 4 && can_move(this, DIRECTION(d)) && !this->confusion;
}

static void lich(Monster *this, Coords d) {
	if (can_cast(this, d) && !player.confusion) {
		player.confusion = 5;
		this->delay = 1;
	} else {
		basic_seek(this, d);
	}
}

static void windmage(Monster *this, Coords d) {
	if (can_cast(this, d)) {
		forced_move(&player, -DIRECTION(d));
		this->delay = 1;
	} else {
		basic_seek(this, d);
	}
}

// Attack in a 3x3 zone without moving.
static void mushroom(Monster *this, Coords d) {
	if (L2(d) < 4)
		enemy_attack(this);
	this->delay = CLASS(this).beat_delay;
}

// Chase the player, then attack in a 3x3 zone.
static void yeti(Monster *this, Coords d) {
	basic_seek(this, d);
	if (L2(player.pos - this->pos) < 4)
		enemy_attack(this);
}


// Move up to 3 tiles toward the player.
// Only attacks if the player was already adjacent. The destination must be visible.
// Try to move as little as possible in L2 distance.
static void harpy(Monster *this, Coords d) {
	static const Coords moves[] = {
		{0, -1}, {-1, 0}, {1, 0}, {0, 1},
		{-1, -1}, {1, -1}, {-1, 1}, {1, 1},
		{0, -2}, {-2, 0}, {2, 0}, {0, 2},
		{-1, -2}, {1, -2}, {-2, -1}, {2, -1}, {-2, 1}, {2, 1}, {-1, 2}, {1, 2},
		{0, -3}, {-3, 0}, {3, 0}, {0, 3},
	};
	if (L1(d) == 1) {
		enemy_move(this, d);
		return;
	}
	Coords best = {0, 0};
	long min = L1(d);
	for (long i = 0; i < LENGTH(moves); ++i) {
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
		long score = L1(d - move);
		if (score && score < min && can_move(this, move)) {
			min = score;
			best = move;
		}
	}
	enemy_move(this, best);
}

static void zombie(Monster *this, __attribute__((unused)) Coords d) {
	static const Coords moves[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	this->state ^= !enemy_move(this, moves[this->state]);
	this->delay = 1;
}

static void blue_slime(Monster *this, __attribute__((unused)) Coords d) {
	static const Coords moves[] = {{0, -1}, {0, 1}, {0, -1}, {0, 1}};
	this->state += enemy_move(this, moves[this->state]);
}

static void yellow_slime(Monster *this, __attribute__((unused)) Coords d) {
	static const Coords moves[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
	this->state += enemy_move(this, moves[this->state]);
}

static void diagonal_slime(Monster *this, __attribute__((unused)) Coords d) {
	static const Coords moves[] = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
	this->state += enemy_move(this, moves[this->state]);
}

// State 0: camouflaged
// State 1: invulnerable (right after waking up)
// State 2: chasing the player
static void mimic(Monster *this, Coords d) {
	if (this->state) {
		this->state = 2;
		basic_seek(this, d);
	} else if (L1(d) == 1) {
		this->state = 1;
	}
}

static bool can_breath(Monster *this) {
	int dx = ABS(player.pos.x - this->pos.x);
	int dy = ABS(player.pos.y - this->pos.y);
	bool red = this->class == RED_DRAGON;
	return this->state < 2 && (red ? !dy : dx < 4 && dy < dx && !player.freeze);
}

static void breath_attack(Monster *this) {
	int8_t direction = SIGN(player.prev_pos.x - this->pos.x);
	(this->class == RED_DRAGON ? fireball : cone_of_cold)(this->pos, direction);
	this->delay = 1;
}

// Dragons normally chase the player cardinally every two beats (see basic_seek).
// However, as soon as the player is in breath range, they’ll charge a breath attack,
// then fire it on the next beat.
// They then resume chasing, but can’t charge another breath in the next two beats.
static void dragon(Monster *this, Coords d) {
	if (this->state == 0 || this->state == 2)
		basic_seek(this, d);
	else if (this->state == 3)
		breath_attack(this);
	this->state = can_breath(this) && can_see(this->pos) ? 3 : ABS(this->state - 1);
}

static void elemental(Monster *this, Coords d) {
	tile_change(&TILE(this->pos), this->class == EFREET ? FIRE : ICE);
	basic_seek(this, d);
	tile_change(&TILE(this->pos), this->class == EFREET ? FIRE : ICE);
}

static void mole(Monster *this, Coords d) {
	if (this->state != (L1(d) == 1))
		this->state ^= 1;
	else
		basic_seek(this, d);
}

static void beetle_shed(Monster *this) {
	tile_change(&TILE(this->pos), this->class == FIRE_BEETLE ? FIRE : ICE);
	this->class = BEETLE;
}

static void beetle(Monster *this, Coords d) {
	if (L1(d) == 1)
		beetle_shed(this);
	basic_seek(this, d);
	if (L1(player.pos - this->pos) == 1)
		beetle_shed(this);
}

static void wind_statue(__attribute__((unused)) Monster *this, Coords d) {
	if (L1(d) == 1)
		forced_move(&player, d);
}

static void bomb_statue(Monster *this, Coords d) {
	if (this->state)
		bomb_tick(this, d);
	else if (L1(d) == 1)
		this->state = 1;
}

static void armadillo(Monster *this, Coords d) {
	if (this->state) {
		if (!enemy_move(this, this->pos - this->prev_pos)) {
			this->delay = 3;
			this->state = 0;
		}
	} else if (d.x * d.y == 0 && can_see(this->pos)) {
		this->state = 1;
		enemy_move(this, DIRECTION(d));
	}
}

static void todo() {}
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
	[GHOST]       = { 1, 0,   9,  true, -1, 10201102, "8",        todo },
	[ZOMBIE]      = { 1, 1, 225, false, -1, 10201201, GREEN "Z",  zombie },
	[WRAITH]      = { 1, 0,   9,  true, -1, 10101102, RED "W",    basic_seek },
	[MIMIC_1]     = { 1, 0,   0, false, -1, 10201100, YELLOW "m", mimic },
	[MIMIC_2]     = { 1, 0,   0, false, -1, 10301100, BLUE "m",   mimic },

	[SKELETANK_1] = { 1, 1,   9, false, -1, 10101202, "Z",        basic_seek },
	[SKELETANK_2] = { 2, 1,   9, false, -1, 10302204, YELLOW "Z", basic_seek },
	[SKELETANK_3] = { 3, 1,   9, false, -1, 10503206, BLACK "Z",  basic_seek },
	[WINDMAGE_1]  = { 1, 1,   9, false, -1, 10201202, BLUE "@",   windmage },
	[WINDMAGE_2]  = { 2, 1,   9, false, -1, 10402204, YELLOW "@", windmage },
	[WINDMAGE_3]  = { 3, 1,   9, false, -1, 10503206, BLACK "@",  windmage },
	[MUSHROOM_1]  = { 1, 3,   9, false, -1, 10201402, BLUE "%",   mushroom },
	[MUSHROOM_2]  = { 3, 2,   9, false, -1, 10403303, PURPLE "%", mushroom },
	[GOLEM_1]     = { 5, 3,   9,  true,  2, 20405404, "'",        basic_seek },
	[GOLEM_2]     = { 7, 3,   9,  true,  2, 20607407, BLACK "'",  basic_seek },
	[ARMADILLO_1] = { 1, 0,   9, false,  2, 10201102, "q",        armadillo },
	[ARMADILLO_2] = { 2, 0,   9, false,  2, 10302105, YELLOW "q", armadillo },
	[CLONE]       = { 1, 0,   9, false, -1, 10301102, "@",        todo },
	[TARMONSTER]  = { 1, 0,   9, false, -1, 10304103, "t",        mimic },
	[MOLE]        = { 1, 0,   9,  true, -1,  1020113, "r",        mole },
	[WIGHT]       = { 1, 0,   9,  true, -1, 10201103, GREEN "W",  basic_seek },
	[WALL_MIMIC]  = { 1, 0,   0, false, -1, 10201103, GREEN "m",  mimic },
	[LIGHTSHROOM] = { 1, 9,   9, false, -1,        0, "%",        nop },
	[BOMBSHROOM]  = { 1, 9,   9, false, -1,      ~1u, "%",        todo },

	[FIRE_SLIME]  = { 1, 0, 225, false,  2, 10301101, RED "P",    diagonal_slime },
	[ICE_SLIME]   = { 1, 0, 225, false,  2, 10301101, CYAN "P",   diagonal_slime },
	[RIDER_1]     = { 1, 0,   9,  true, -1, 10201102, "&",        basic_seek },
	[RIDER_2]     = { 2, 0,   9,  true, -1, 10402104, YELLOW "&", basic_seek },
	[RIDER_3]     = { 3, 0,   9,  true, -1, 10603106, BLACK "&",  basic_seek },
	[EFREET]      = { 2, 2,   9,  true,  2, 20302302, RED "E",    elemental },
	[DJINN]       = { 2, 2,   9,  true,  2, 20302302, CYAN "E",   elemental },
	[ASSASSIN_1]  = { 1, 0,   9, false, -1, 10401103, PURPLE "G", todo },
	[ASSASSIN_2]  = { 2, 0,   9, false, -1, 10602105, BLACK "G",  todo },
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
	[DIGGER]      = { 1, 1,   9, false,  2, 10101201, "G",        basic_seek },
	[BLACK_BAT]   = { 1, 0,   9,  true, -1, 10401120, BLACK "B",  black_bat },
	[ARMADILDO]   = { 3, 0,   9, false,  2, 10303104, ORANGE "q", todo },
	[BLADENOVICE] = { 1, 1,   9, false, -1, 99999995, "b",        blademaster },
	[BLADEMASTER] = { 2, 1,   9, false, -1, 99999996, "b",        blademaster },
	[GHOUL]       = { 1, 0,   9,  true, -1, 10301102, "W",        moore_seek },
	[OOZE_GOLEM]  = { 5, 3,   9,  true,  2, 20510407, GREEN "'",  basic_seek },
	[HARPY]       = { 1, 1,   0,  true, -1, 10301203, GREEN "h",  harpy },
	[LICH_1]      = { 1, 1,   9, false, -1, 10404202, GREEN "L",  lich },
	[LICH_2]      = { 2, 1,   9, false, -1, 10404302, PURPLE "L", lich },
	[LICH_3]      = { 3, 1,   9, false, -1, 10404402, BLACK "L",  lich },
	[CONF_MONKEY] = { 1, 0,   9, false, -1, 10004103, GREEN "Y",  basic_seek },
	[TELE_MONKEY] = { 2, 0,   9, false, -1, 10002103, PINK "Y",   basic_seek },
	[PIXIE]       = { 1, 0,   9,  true, -1, 10401102, "n",        basic_seek },
	[SARCO_1]     = { 1, 9,   9, false, -1, 10101805, "|",        todo },
	[SARCO_2]     = { 2, 9,   9, false, -1, 10102910, YELLOW "|", todo },
	[SARCO_3]     = { 3, 9,   9, false, -1, 10103915, BLACK "|",  todo },
	[SPIDER]      = { 1, 1,   9, false, -1, 10401202, YELLOW "s", basic_seek },
	[FREE_SPIDER] = { 1, 0,   9, false, -1, 10401202, YELLOW "s", diagonal_seek },
	[WARLOCK_1]   = { 1, 1,   9, false, -1, 10401202, "w",        basic_seek },
	[WARLOCK_2]   = { 2, 1,   9, false, -1, 10401302, YELLOW "w", basic_seek },
	[MUMMY]       = { 1, 1,   9, false, -1, 30201103, "M",        moore_seek },
	[WIND_STATUE] = { 1, 0,   0, false, -1, 10401102, CYAN "g",   wind_statue },
	[SEEK_STATUE] = { 1, 0,   0, false, -1, 10401102, BLACK "g",  mimic },
	[BOMB_STATUE] = { 1, 0,   0, false, -1, 10401102, YELLOW "g", bomb_statue },
	[MINE_STATUE] = { 1, 0,   0, false, -1, 10401102, RED "g",    nop },
	[CRATE_1]     = { 1, 1,   0, false, -1, 10401102, "g",        nop },
	[CRATE_2]     = { 1, 1,   0, false, -1, 10401102, "g",        nop },

	[SHOPKEEPER]  = { 9, 9,   9, false, -1, 99999997, "@",        nop },
	[DIREBAT_1]   = { 2, 1,   9,  true, -1, 30302210, YELLOW "B", bat },
	[DIREBAT_2]   = { 3, 1,   9,  true, -1, 30403215, "B",        bat },
	[DRAGON]      = { 4, 1,  49,  true,  4, 30404210, GREEN "D",  basic_seek },
	[RED_DRAGON]  = { 6, 0, 225,  true,  4, 99999999, RED "D",    dragon },
	[BLUE_DRAGON] = { 6, 0,   0,  true,  4, 99999997, BLUE "D",   dragon },
	[BANSHEE_1]   = { 3, 0,  49,  true, -1, 30403110, BLUE "8",   basic_seek },
	[BANSHEE_2]   = { 4, 0,  49,  true, -1, 30604115, GREEN "8",  basic_seek },
	[MINOTAUR_1]  = { 3, 0,  49,  true,  2, 30403110, "H",        todo },
	[MINOTAUR_2]  = { 5, 0,  49,  true,  2, 30505115, BLACK "H",  todo },
	[NIGHTMARE_1] = { 3, 1,  49,  true,  4, 30403210, BLACK "u",  basic_seek },
	[NIGHTMARE_2] = { 5, 1,  49,  true,  4, 30505215, RED "u",    basic_seek },
	[MOMMY]       = { 6, 3,  49,  true, -1, 30405215, BLACK "@",  basic_seek },
	[OGRE]        = { 5, 3,  49,  true,  2, 30505115, GREEN "O",  basic_seek },

	[PLAYER]      = { 1, 0,   0, false, -1,      ~0u, "@",        NULL },
	[BOMB]        = { 0, 0,   0, false, -1,      ~1u, "o",        bomb_tick },
};
