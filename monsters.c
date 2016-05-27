static void basic_seek(Monster *this, long dy, long dx) {
	this->vertical =
		// #1: move towards the player
		dy == 0 ? 0 :
		dx == 0 ? 1 :

		// #2: avoid obstacles
		!can_move(this, SIGN(dy), 0) ? 0 :
		!can_move(this, 0, SIGN(dx)) ? 1 :
	
		// #3: move towards the player’s previous position
		this->y == player.prev_y ? 0 :
		this->x == player.prev_x ? 1 :

		// #4: if prevpos aligns with the player, switch axes
		this->prev_y == player.y ? 0 :
		this->prev_x == player.x ? 1 :

		// #5: don’t switch axes for a single tile
		ABS(dy) == 1 || ABS(dx) == 1 ? this->vertical :

		// #6: if prevpos aligns with the player’s prevpos, do something weird
		this->prev_y == player.prev_y ? dx > 0 && player.x > SPAWN_X :
		this->prev_x == player.prev_x ? dx > 0 && player.x > SPAWN_X :

		// #7: keep moving along the same axis
		this->vertical;

	monster_move(this, this->vertical ? SIGN(dy) : 0, this->vertical ? 0 : SIGN(dx));
}

static void diagonal_seek(Monster *this, long dy, long dx) {
	if (dy == 0)
		monster_move(this, 1, SIGN(dx)) || monster_move(this, -1, SIGN(dx));
	else if (dx == 0)
		monster_move(this, SIGN(dy), 1) || monster_move(this, SIGN(dy), -1);
	else
		monster_move(this, SIGN(dy), SIGN(dx)) ||
		monster_move(this, SIGN(dy) * -SIGN(dx), 1) ||
		monster_move(this, SIGN(dy) * SIGN(dx), -1);
}

static void moore_seek(Monster *this, long dy, long dx) {
	if (monster_move(this, SIGN(dy), SIGN(dx)))
		return;
	if (dx < 0)
		monster_move(this, 0, -1) || monster_move(this, SIGN(dy), 0);
	else
		monster_move(this, SIGN(dy), 0) || monster_move(this, 0, 1);
}

static void bat(Monster *this, long dy, long dx) {
	(void) dy;
	(void) dx;
	static const int8_t bat_y[4] = {0, 0,  1, -1};
	static const int8_t bat_x[4] = {1, -1, 0, 0};
	long rng = rand();
	for (long i = 0; i < 4; ++i)
		if (monster_move(this, bat_y[(rng + i) & 3], bat_x[(rng + i) & 3]))
			return;
}

static void black_bat(Monster *this, long dy, long dx) {
	if (ABS(dy) + ABS(dx) == 1)
		monster_move(this, (int8_t) dy, (int8_t) dx);
	else
		bat(this, dy, dx);
}

static void parry(Monster *this, long dy, long dx) {
	if (this->state == 0) {
		basic_seek(this, dy, dx);
	} else if (this->state == 1) {
		int8_t y = SIGN(player.prev_y - this->y);
		int8_t x = SIGN(player.prev_x - this->x);
		if (monster_move(this, y, x))
			monster_move(this, y, x);
		this->state = 2;
		this->delay = 0;
	} else if (this->state == 2) {
		this->state = 0;
	}
}

static void todo() {}
static void nop() {}

static ClassInfos class_infos[256] = {
	[GREEN_SLIME] = { 1, 9, 19901101, GREEN "P",  nop },
	[BLUE_SLIME]  = { 2, 1, 10202202, BLUE "P",   todo },
	[YOLO_SLIME]  = { 1, 0, 10101102, YELLOW "P", todo },
	[SKELETON_1]  = { 1, 1, 10101202, "Z",        basic_seek },
	[SKELETON_2]  = { 2, 1, 10302203, YELLOW "Z", basic_seek },
	[SKELETON_3]  = { 3, 1, 10403204, BLACK "Z",  basic_seek },
	[BLUE_BAT]    = { 1, 1, 10101202, BLUE "B",   bat },
	[RED_BAT]     = { 1, 0, 10201103, RED "B",    bat },
	[GREEN_BAT]   = { 1, 0, 10301120, GREEN "B",  bat },
	[MONKEY_1]    = { 1, 0, 10004101, PURPLE "Y", basic_seek },
	[MONKEY_2]    = { 2, 0, 10006103, "Y",        basic_seek },
	[GHOST]       = { 1, 0, 10201102, "8",        todo },
	[ZOMBIE]      = { 1, 1, 10201201, GREEN "Z",  todo },
	[WRAITH]      = { 1, 0, 10101102, RED "W",    basic_seek },

	[SKELETANK_1] = { 1, 1, 10101202, "Z",        basic_seek },
	[SKELETANK_2] = { 2, 1, 10302204, YELLOW "Z", basic_seek },
	[SKELETANK_3] = { 3, 1, 10503206, BLACK "Z",  basic_seek },
	[WINDMAGE_1]  = { 1, 1, 10201202, BLUE "@",   todo },
	[WINDMAGE_2]  = { 2, 1, 10402204, YELLOW "@", todo },
	[WINDMAGE_3]  = { 3, 1, 10503206, BLACK "@",  todo },
	[MUSHROOM_1]  = { 1, 3, 10201402, BLUE "%",   todo },
	[MUSHROOM_2]  = { 3, 2, 10403303, PURPLE "%", todo },
	[GOLEM_1]     = { 5, 3, 20405404, "'",        basic_seek },
	[GOLEM_2]     = { 7, 3, 20607407, BLACK "'",  basic_seek },
	[ARMADILLO_1] = { 1, 0, 10201102, "q",        todo },
	[ARMADILLO_2] = { 2, 0, 10302105, YELLOW "q", todo },
	[CLONE]       = { 1, 0, 10301102, "@",        todo },
	[TARMONSTER]  = { 1, 0, 10304103, "t",        todo },
	[MOLE]        = { 1, 0,  1020113, "r",        todo },
	[WIGHT]       = { 1, 0, 10201103, GREEN "W",  todo },
	[WALL_MIMIC]  = { 1, 0, 10201103, GREEN "m",  todo },
	[LIGHTSHROOM] = { 1, 9,        0, "%",        nop },
	[BOMBSHROOM]  = { 1, 9,      ~1u, "%",        todo },

	[FIRE_SLIME]  = { 1, 0, 10301101, RED "P",    todo },
	[ICE_SLIME]   = { 1, 0, 10301101, CYAN "P",   todo },
	[RIDER_1]     = { 1, 0, 10201102, "&",        basic_seek },
	[RIDER_2]     = { 2, 0, 10402104, YELLOW "&", basic_seek },
	[RIDER_3]     = { 3, 0, 10603106, BLACK "&",  basic_seek },
	[EFREET]      = { 2, 2, 20302302, RED "E",    basic_seek },
	[DJINN]       = { 2, 2, 20302302, CYAN "E",   basic_seek },
	[ASSASSIN_1]  = { 1, 0, 10401103, PURPLE "G", todo },
	[ASSASSIN_2]  = { 2, 0, 10602105, GRAY "G",   todo },
	[FIRE_BEETLE] = { 3, 1, 10303202, RED "a",    basic_seek },
	[ICE_BEETLE]  = { 3, 1, 10303202, CYAN "a",   basic_seek },
	[HELLHOUND]   = { 1, 1, 10301202, RED "d",    moore_seek },
	[SHOVE_1]     = { 2, 0, 10002102, PURPLE "~", basic_seek },
	[SHOVE_2]     = { 3, 0, 10003102, GRAY "~",   basic_seek },
	[YETI]        = { 1, 3, 20301403, CYAN "Y",   basic_seek },
	[GHAST]       = { 1, 0, 10201102, PURPLE "W", basic_seek },
	[FIRE_MIMIC]  = { 1, 0, 10201102, RED "m",    todo },
	[ICE_MIMIC]   = { 1, 0, 10201102, CYAN "m",   todo },
	[FIRE_POT]    = { 1, 9,        0, RED "(",    nop },
	[ICE_POT]     = { 1, 0,        0, CYAN "(",   nop },

	[BOMBER]      = { 1, 1, 99999998, RED "G",    diagonal_seek },
	[DIGGER]      = { 1, 1, 10101201, "G",        basic_seek },
	[BLACK_BAT]   = { 1, 0, 10401120, BLACK "B",  black_bat },
	[ARMADILDO]   = { 3, 0, 10303104, ORANGE "q", todo },
	[BLADENOVICE] = { 1, 1, 99999995, "b",        parry },
	[BLADEMASTER] = { 2, 1, 99999996, "b",        parry },
	[GHOUL]       = { 1, 0, 10301102, "W",        moore_seek },
	[OOZE_GOLEM]  = { 5, 3, 20510407, GREEN "'",  basic_seek },
	[HARPY]       = { 1, 1, 10301203, GREEN "h",  basic_seek },
	[LICH_1]      = { 1, 1, 10404202, "L",        basic_seek },
	[LICH_2]      = { 2, 1, 10404302, PURPLE "L", basic_seek },
	[LICH_3]      = { 3, 1, 10404402, BLACK "L",  basic_seek },
	[CONF_MONKEY] = { 1, 0, 10004103, GREEN "Y",  basic_seek },
	[TELE_MONKEY] = { 2, 0, 10002103, PINK "Y",   basic_seek },
	[PIXIE]       = { 1, 0, 10401102, "n",        basic_seek },
	[SARCO_1]     = { 1, 9, 10101805, "|",        todo },
	[SARCO_2]     = { 2, 9, 10102910, YELLOW "|", todo },
	[SARCO_3]     = { 3, 9, 10103915, BLACK "|",  todo },
	[SPIDER]      = { 1, 1, 10401202, YELLOW "s", basic_seek },
	[WARLOCK_1]   = { 1, 1, 10401202, "w",        basic_seek },
	[WARLOCK_2]   = { 2, 1, 10401302, YELLOW "w", basic_seek },
	[MUMMY]       = { 1, 1, 30201103, "M",        moore_seek },
	[GARGOYLE_1]  = { 1, 1, 10401102, "g",        todo },
	[GARGOYLE_2]  = { 1, 1, 10401102, "g",        todo },
	[GARGOYLE_3]  = { 1, 1, 10401102, "g",        todo },
	[GARGOYLE_4]  = { 1, 1, 10401102, "g",        todo },
	[GARGOYLE_5]  = { 1, 1, 10401102, "g",        todo },
	[GARGOYLE_6]  = { 1, 1, 10401102, "g",        todo },

	[SHOPKEEPER]  = { 9, 9, 99999997, "@",        nop },
	[DIREBAT_1]   = { 2, 1, 30302210, YELLOW "B", bat },
	[DIREBAT_2]   = { 3, 1, 30403215, GRAY "B",   bat },
	[DRAGON]      = { 4, 1, 30404210, GREEN "D",  basic_seek },
	[RED_DRAGON]  = { 6, 1, 99999999, RED "D",    basic_seek },
	[BLUE_DRAGON] = { 6, 1, 99999997, BLUE "D",   basic_seek },
	[BANSHEE_1]   = { 3, 0, 30403110, BLUE "8",   basic_seek },
	[BANSHEE_2]   = { 4, 0, 30604115, GREEN "8",  basic_seek },
	[MINOTAUR_1]  = { 3, 0, 30403110, "H",        todo },
	[MINOTAUR_2]  = { 5, 0, 30505115, GRAY "H",   todo },
	[NIGHTMARE_1] = { 3, 1, 30403210, BLACK "u",  basic_seek },
	[NIGHTMARE_2] = { 5, 1, 30505215, RED "u",    basic_seek },
	[MOMMY]       = { 6, 3, 30405215, BLACK "@",  basic_seek },
	[OGRE]        = { 5, 3, 30505115, GREEN "O",  basic_seek },

	[PLAYER]      = { 0, 0,      ~0u, "@",        NULL },
};
