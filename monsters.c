// monsters.c - defines all monsters in the game and their AIs

// Many things in the game follow the so-called “bomb order”:
// 147
// 258
// 369
// Most monster AIs use this as a tiebreaker: when several destination tiles fit
// all criteria equally well, monsters pick the one that comes first in bomb-order.

// Move cardinally toward the player.
// Try to keep moving along the same axis, unless the monster’s current or
// previous position is aligned with the player’s current or previous position.
static void basic_seek(Monster *this, long dy, long dx) {
	this->vertical =
		// #1: move toward the player
		dy == 0 ? 0 :
		dx == 0 ? 1 :

		// #2: avoid obstacles
		!can_move(this, SIGN(dy), 0) ? 0 :
		!can_move(this, 0, SIGN(dx)) ? 1 :
	
		// #3: move toward the player’s previous position
		this->y == player.prev_y ? 0 :
		this->x == player.prev_x ? 1 :

		// #4: if prevpos aligns with the player, switch axes
		this->prev_y == player.y ? 0 :
		this->prev_x == player.x ? 1 :

		// #5: don’t switch axes for a single tile
		ABS(dy) == 1 || ABS(dx) == 1 ? this->vertical :

		// #6: if prevpos aligns with the player’s prevpos, do something weird
		this->prev_y == player.prev_y ? dx > 0 && player.x > SPAWN_X :
		this->prev_x == player.prev_x ? ABS(dy) != 2 :

		// #7: keep moving along the same axis
		this->vertical;

	enemy_move(this, this->vertical ? SIGN(dy) : 0, this->vertical ? 0 : SIGN(dx));
}

// Move diagonally toward the player. The tiebreaker is *reverse* bomb-order.
static void diagonal_seek(Monster *this, long dy, long dx) {
	if (dy == 0)
		enemy_move(this, 1, SIGN(dx)) || enemy_move(this, -1, SIGN(dx));
	else if (dx == 0)
		enemy_move(this, SIGN(dy), 1) || enemy_move(this, SIGN(dy), -1);
	else
		enemy_move(this, SIGN(dy), SIGN(dx)) ||
		enemy_move(this, SIGN(dy) * -SIGN(dx), 1) ||
		enemy_move(this, SIGN(dy) * SIGN(dx), -1);
}

// Move toward the player either cardinally or diagonally.
static void moore_seek(Monster *this, long dy, long dx) {
	if (enemy_move(this, SIGN(dy), SIGN(dx)))
		return;
	if (dx < 0)
		enemy_move(this, 0, -1) || enemy_move(this, SIGN(dy), 0);
	else
		enemy_move(this, SIGN(dy), 0) || enemy_move(this, 0, 1);
}

// Move randomly.
static void bat(Monster *this, __attribute__((unused)) long dy, __attribute__((unused)) long dx) {
	static const int8_t bat_y[4] = {0, 0,  1, -1};
	static const int8_t bat_x[4] = {1, -1, 0, 0};
	long rng = rand();
	for (long i = 0; i < 4; ++i)
		if (enemy_move(this, bat_y[(rng + i) & 3], bat_x[(rng + i) & 3]))
			return;
}

// Attack the player if possible, otherwise move randomly.
static void black_bat(Monster *this, long dy, long dx) {
	if (L1(dy, dx) == 1)
		enemy_move(this, (int8_t) dy, (int8_t) dx);
	else
		bat(this, dy, dx);
}

// basic_seek variant used by blademasters.
// After parrying a melee hit, lunge two tiles in the direction the hit came from.
static void parry(Monster *this, long dy, long dx) {
	if (this->state == 0) {
		basic_seek(this, dy, dx);
	} else if (this->state == 1) {
		int8_t y = SIGN(player.prev_y - this->y);
		int8_t x = SIGN(player.prev_x - this->x);
		enemy_move(this, y, x);
		enemy_move(this, y, x);
		this->state = 2;
		this->delay = 0;
	} else if (this->state == 2) {
		this->state = 0;
	}
}

static void lich(Monster *this, long dy, long dx) {
	if (L2(dy, dx) == 4 && !player.confused) {
		player.confused = 4;
		this->delay = 1;
	} else {
		basic_seek(this, dy, dx);
	}
}

static void windmage(Monster *this, long dy, long dx) {
	if (L2(dy, dx) == 4 && can_move(this, SIGN(dy), SIGN(dx))) {
		forced_move(&player, -SIGN(dy), -SIGN(dx));
		this->delay = 1;
	} else {
		basic_seek(this, dy, dx);
	}
}

static void mushroom(Monster *this, long dy, long dx) {
	if (L2(dy, dx) < 4)
		enemy_attack(this);
	this->delay = CLASS(this).beat_delay;
}

typedef struct { int8_t y; int8_t x; } Distance;

static const Distance harpy_moves[] = {
	{0, -1}, {-1, 0}, {1, 0}, {0, 1},
	{-1, -1}, {1, -1}, {-1, 1}, {1, 1},
	{0, -2}, {-2, 0}, {2, 0}, {0, 2},
	{-1, -2}, {1, -2}, {-2, -1}, {2, -1}, {-2, 1}, {2, 1}, {-1, 2}, {1, 2},
	{0, -3}, {-3, 0}, {3, 0}, {0, 3},
};

static void harpy(Monster *this, long dy, long dx) {
	if (L1(dy, dx) == 1) {
		enemy_move(this, (int8_t) dy, (int8_t) dx);
		return;
	}
	const Distance *best = &(Distance) {0, 0};
	long min = L1(dy, dx);
	for (const Distance *d = harpy_moves; d < harpy_moves + LENGTH(harpy_moves); ++d) {
		if ((L2(d->y, d->x) == 9 || L2(d->y, d->x) == 4) && (
				IS_OPAQUE(this->y +   SIGN(d->y), this->x +   SIGN(d->x)) ||
				IS_OPAQUE(this->y + 2*SIGN(d->y), this->x + 2*SIGN(d->x))))
			continue;
		if (L2(d->y, d->x) == 5 &&
				IS_OPAQUE(this->y + SIGN(d->y), this->x + SIGN(d->x)) &&
				IS_OPAQUE(this->y + d->y / 2, this->x + d->x / 2))
			continue;
		long score = L1(dy - d->y, dx - d->x);
		if (score && score < min && can_move(this, d->y, d->x)) {
			min = score;
			best = d;
		}
	}
	enemy_move(this, best->y, best->x);
}

static void blue_slime(Monster *this, __attribute__((unused)) long dy, __attribute__((unused)) long dx) {
	this->state ^= enemy_move(this, this->state ? -1 : 1, 0);
}

static void yellow_slime(Monster *this, __attribute__((unused)) long dy, __attribute__((unused)) long dx) {
	static const int8_t move_y[] = {0, 1, 0, -1};
	static const int8_t move_x[] = {1, 0, -1, 0};
	this->state += enemy_move(this, move_y[this->state], move_x[this->state]);
}

static void diagonal_slime(Monster *this, __attribute__((unused)) long dy, __attribute__((unused)) long dx) {
	static const int8_t move_y[] = {1, 1, -1, -1};
	static const int8_t move_x[] = {1, -1, -1, 1};
	bool ok = can_move(this, move_y[this->state], move_x[this->state]);
	enemy_move(this, move_y[this->state], move_x[this->state]);
	this->state += ok;
}

static void todo() {}
static void mimic() {/*TODO*/}
static void nop() {}

static const ClassInfos class_infos[256] = {
	// [Name] = { max_hp, beat_delay, radius, flying, dig, priority, glyph, act }
	[GREEN_SLIME] = { 1, 9, 225, false, 0, 19901101, GREEN "P",  nop },
	[BLUE_SLIME]  = { 2, 1, 225, false, 0, 10202202, BLUE "P",   blue_slime },
	[YOLO_SLIME]  = { 1, 0, 225, false, 0, 10101102, YELLOW "P", yellow_slime },
	[SKELETON_1]  = { 1, 1,   9, false, 0, 10101202, "Z",        basic_seek },
	[SKELETON_2]  = { 2, 1,   9, false, 0, 10302203, YELLOW "Z", basic_seek },
	[SKELETON_3]  = { 3, 1,   9, false, 0, 10403204, BLACK "Z",  basic_seek },
	[BLUE_BAT]    = { 1, 1,   9,  true, 0, 10101202, BLUE "B",   bat },
	[RED_BAT]     = { 1, 0,   9,  true, 0, 10201103, RED "B",    bat },
	[GREEN_BAT]   = { 1, 0,   9,  true, 0, 10301120, GREEN "B",  bat },
	[MONKEY_1]    = { 1, 0,   9, false, 0, 10004101, PURPLE "Y", basic_seek },
	[MONKEY_2]    = { 2, 0,   9, false, 0, 10006103, "Y",        basic_seek },
	[GHOST]       = { 1, 0,   9,  true, 0, 10201102, "8",        todo },
	[ZOMBIE]      = { 1, 1,   9, false, 0, 10201201, GREEN "Z",  todo },
	[WRAITH]      = { 1, 0,   9,  true, 0, 10101102, RED "W",    basic_seek },

	[SKELETANK_1] = { 1, 1,   9, false, 0, 10101202, "Z",        basic_seek },
	[SKELETANK_2] = { 2, 1,   9, false, 0, 10302204, YELLOW "Z", basic_seek },
	[SKELETANK_3] = { 3, 1,   9, false, 0, 10503206, BLACK "Z",  basic_seek },
	[WINDMAGE_1]  = { 1, 1,   9, false, 0, 10201202, BLUE "@",   windmage },
	[WINDMAGE_2]  = { 2, 1,   9, false, 0, 10402204, YELLOW "@", windmage },
	[WINDMAGE_3]  = { 3, 1,   9, false, 0, 10503206, BLACK "@",  windmage },
	[MUSHROOM_1]  = { 1, 3,   9, false, 0, 10201402, BLUE "%",   mushroom },
	[MUSHROOM_2]  = { 3, 2,   9, false, 0, 10403303, PURPLE "%", mushroom },
	[GOLEM_1]     = { 5, 3,  49,  true, 2, 20405404, "'",        basic_seek },
	[GOLEM_2]     = { 7, 3,  49,  true, 2, 20607407, BLACK "'",  basic_seek },
	[ARMADILLO_1] = { 1, 0,   9, false, 2, 10201102, "q",        todo },
	[ARMADILLO_2] = { 2, 0,   9, false, 2, 10302105, YELLOW "q", todo },
	[CLONE]       = { 1, 0,   9, false, 0, 10301102, "@",        todo },
	[TARMONSTER]  = { 1, 0,   9, false, 0, 10304103, "t",        mimic },
	[MOLE]        = { 1, 0,   9,  true, 0,  1020113, "r",        todo },
	[WIGHT]       = { 1, 0,   9,  true, 0, 10201103, GREEN "W",  basic_seek },
	[WALL_MIMIC]  = { 1, 0,   9, false, 0, 10201103, GREEN "m",  mimic },
	[LIGHTSHROOM] = { 1, 9,   9, false, 0,        0, "%",        nop },
	[BOMBSHROOM]  = { 1, 9,   9, false, 0,      ~1u, "%",        todo },

	[FIRE_SLIME]  = { 1, 0, 225, false, 2, 10301101, RED "P",    diagonal_slime },
	[ICE_SLIME]   = { 1, 0, 225, false, 2, 10301101, CYAN "P",   diagonal_slime },
	[RIDER_1]     = { 1, 0,   9,  true, 0, 10201102, "&",        basic_seek },
	[RIDER_2]     = { 2, 0,   9,  true, 0, 10402104, YELLOW "&", basic_seek },
	[RIDER_3]     = { 3, 0,   9,  true, 0, 10603106, BLACK "&",  basic_seek },
	[EFREET]      = { 2, 2,   9,  true, 2, 20302302, RED "E",    basic_seek },
	[DJINN]       = { 2, 2,   9,  true, 2, 20302302, CYAN "E",   basic_seek },
	[ASSASSIN_1]  = { 1, 0,   9, false, 0, 10401103, PURPLE "G", todo },
	[ASSASSIN_2]  = { 2, 0,   9, false, 0, 10602105, BLACK "G",  todo },
	[FIRE_BEETLE] = { 3, 1,  49, false, 0, 10303202, RED "a",    basic_seek },
	[ICE_BEETLE]  = { 3, 1,  49, false, 0, 10303202, CYAN "a",   basic_seek },
	[HELLHOUND]   = { 1, 1,   9, false, 0, 10301202, RED "d",    moore_seek },
	[SHOVE_1]     = { 2, 0,   9, false, 0, 10002102, PURPLE "~", basic_seek },
	[SHOVE_2]     = { 3, 0,   9, false, 0, 10003102, BLACK "~",  basic_seek },
	[YETI]        = { 1, 3,   9,  true, 2, 20301403, CYAN "Y",   basic_seek },
	[GHAST]       = { 1, 0,   9,  true, 0, 10201102, PURPLE "W", basic_seek },
	[FIRE_MIMIC]  = { 1, 0,   9, false, 0, 10201102, RED "m",    mimic },
	[ICE_MIMIC]   = { 1, 0,   9, false, 0, 10201102, CYAN "m",   mimic },
	[FIRE_POT]    = { 1, 9,   9, false, 0,        0, RED "(",    nop },
	[ICE_POT]     = { 1, 0,   9, false, 0,        0, CYAN "(",   nop },

	[BOMBER]      = { 1, 1,   9, false, 0, 99999998, RED "G",    diagonal_seek },
	[DIGGER]      = { 1, 1,   9, false, 2, 10101201, "G",        basic_seek },
	[BLACK_BAT]   = { 1, 0,   9,  true, 0, 10401120, BLACK "B",  black_bat },
	[ARMADILDO]   = { 3, 0,   9, false, 2, 10303104, ORANGE "q", todo },
	[BLADENOVICE] = { 1, 1,   9, false, 0, 99999995, "b",        parry },
	[BLADEMASTER] = { 2, 1,   9, false, 0, 99999996, "b",        parry },
	[GHOUL]       = { 1, 0,   9,  true, 0, 10301102, "W",        moore_seek },
	[OOZE_GOLEM]  = { 5, 3,  49,  true, 2, 20510407, GREEN "'",  basic_seek },
	[HARPY]       = { 1, 1,   9,  true, 0, 10301203, GREEN "h",  harpy },
	[TEST]        = { 1, 1,   9,  true, 0, 10301203, GREEN "h",  harpy },
	[LICH_1]      = { 1, 1,   9, false, 0, 10404202, "L",        lich },
	[LICH_2]      = { 2, 1,   9, false, 0, 10404302, PURPLE "L", lich },
	[LICH_3]      = { 3, 1,   9, false, 0, 10404402, BLACK "L",  lich },
	[CONF_MONKEY] = { 1, 0,   9, false, 0, 10004103, GREEN "Y",  basic_seek },
	[TELE_MONKEY] = { 2, 0,   9, false, 0, 10002103, PINK "Y",   basic_seek },
	[PIXIE]       = { 1, 0,   9,  true, 0, 10401102, "n",        basic_seek },
	[SARCO_1]     = { 1, 9,   9, false, 0, 10101805, "|",        todo },
	[SARCO_2]     = { 2, 9,   9, false, 0, 10102910, YELLOW "|", todo },
	[SARCO_3]     = { 3, 9,   9, false, 0, 10103915, BLACK "|",  todo },
	[SPIDER]      = { 1, 1,   9, false, 0, 10401202, YELLOW "s", basic_seek },
	[FREE_SPIDER] = { 1, 0,   9, false, 0, 10401202, YELLOW "s", diagonal_seek },
	[WARLOCK_1]   = { 1, 1,   9, false, 0, 10401202, "w",        basic_seek },
	[WARLOCK_2]   = { 2, 1,   9, false, 0, 10401302, YELLOW "w", basic_seek },
	[MUMMY]       = { 1, 1,   9, false, 0, 30201103, "M",        moore_seek },
	[GARGOYLE_1]  = { 1, 1,   9, false, 0, 10401102, "g",        todo },
	[GARGOYLE_2]  = { 1, 1,   9, false, 0, 10401102, "g",        todo },
	[GARGOYLE_3]  = { 1, 1,   9, false, 0, 10401102, "g",        todo },
	[GARGOYLE_4]  = { 1, 1,   9, false, 0, 10401102, "g",        todo },
	[GARGOYLE_5]  = { 1, 1,   9, false, 0, 10401102, "g",        todo },
	[GARGOYLE_6]  = { 1, 1,   9, false, 0, 10401102, "g",        todo },

	[SHOPKEEPER]  = { 9, 9,   9, false, 0, 99999997, "@",        nop },
	[DIREBAT_1]   = { 2, 1,   9,  true, 0, 30302210, YELLOW "B", bat },
	[DIREBAT_2]   = { 3, 1,   9,  true, 0, 30403215, "B",        bat },
	[DRAGON]      = { 4, 1,  49,  true, 4, 30404210, GREEN "D",  basic_seek },
	[RED_DRAGON]  = { 6, 1, 225,  true, 4, 99999999, RED "D",    basic_seek },
	[BLUE_DRAGON] = { 6, 1,   0,  true, 4, 99999997, BLUE "D",   basic_seek },
	[BANSHEE_1]   = { 3, 0,  49,  true, 0, 30403110, BLUE "8",   basic_seek },
	[BANSHEE_2]   = { 4, 0,  49,  true, 0, 30604115, GREEN "8",  basic_seek },
	[MINOTAUR_1]  = { 3, 0,  49,  true, 2, 30403110, "H",        todo },
	[MINOTAUR_2]  = { 5, 0,  49,  true, 2, 30505115, BLACK "H",  todo },
	[NIGHTMARE_1] = { 3, 1,  49,  true, 4, 30403210, BLACK "u",  basic_seek },
	[NIGHTMARE_2] = { 5, 1,  49,  true, 4, 30505215, RED "u",    basic_seek },
	[MOMMY]       = { 6, 3,  49,  true, 0, 30405215, BLACK "@",  basic_seek },
	[OGRE]        = { 5, 3,  49,  true, 2, 30505115, GREEN "O",  basic_seek },

	[PLAYER]      = { 0, 0,   9, false, 0,      ~0u, "@",        NULL },
};
