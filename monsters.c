static void basic_seek(Entity *this) {
	this->vertical =
		// #1: move towards the player
		dy == 0 ? 0 :
		dx == 0 ? 1 :

		// #2: avoid obstacles
		!can_move(this, SIGN(dy), 0) ? 0 :
		!can_move(this, 0, SIGN(dx)) ? 1 :
	
		// #3: move towards the player’s previous position
		this->y == player->prev_y ? 0 :
		this->x == player->prev_x ? 1 :

		// #4: if prevpos aligns with the player, switch axes
		this->prev_y == player->y ? 0 :
		this->prev_x == player->x ? 1 :

		// #5: don’t switch axes for a single tile
		ABS(dy) == 1 || ABS(dx) == 1 ? this->vertical :

		// #6: if prevpos aligns with the player’s prevpos, do something weird
		this->prev_y == player->prev_y ? dx > 0 && player->x > SPAWN_X :
		this->prev_x == player->prev_x ? dx > 0 && player->x > SPAWN_X :

		// #7: keep moving along the same axis
		this->vertical;

	monster_move(this, this->vertical ? SIGN(dy) : 0, this->vertical ? 0 : SIGN(dx));
}

static void diagonal_seek(Entity *this) {
	if (dy == 0)
		monster_move(this, 1, SIGN(dx)) || monster_move(this, -1, SIGN(dx));
	else if (dx == 0)
		monster_move(this, SIGN(dy), 1) || monster_move(this, SIGN(dy), 1);
	else
		monster_move(this, SIGN(dy), SIGN(dx)) ||
		monster_move(this, SIGN(dy) * -SIGN(dx), 1) ||
		monster_move(this, SIGN(dy) * SIGN(dx), -1);
}

static void moore_seek(Entity *this) {
	if (monster_move(this, SIGN(dy), SIGN(dx)))
		return;
	if (dx < 0)
		monster_move(this, 0, -1) || monster_move(this, SIGN(dy), 0);
	else
		monster_move(this, SIGN(dy), 0) || monster_move(this, 0, 1);
}

static void bat(Entity *this) {
	static const int8_t bat_y[4] = {0, 0,  1, -1};
	static const int8_t bat_x[4] = {1, -1, 0, 0};
	int rng = rand();
	for (int i = 0; i < 4; ++i)
		if (monster_move(this, bat_y[(rng + i) & 3], bat_x[(rng + i) & 3]))
			return;
}

static void black_bat(Entity *this) {
	if (ABS(dy) + ABS(dx) == 1)
		monster_move(this, (int8_t) dy, (int8_t) dx);
	else
		bat(this);
}

static void parry(Entity *this) {
	if (this->state == 0) {
		basic_seek(this);
	} else if (this->state == 1) {
		int8_t y = SIGN(player->prev_y - this->y);
		int8_t x = SIGN(player->prev_x - this->x);
		if (monster_move(this, y, x))
			monster_move(this, y, x);
		this->state = 2;
		this->delay = 0;
	} else if (this->state == 2) {
		this->state = 0;
	}
}

static void spike_trap(Entity *this) {
	Entity *target = board[this->y][this->x].next;
	if (target == this)
		return;
	target->hp = 0;
	ent_rm(target);
}

static void nop() {}

static ClassInfos class_infos[256] = {
	[SKELETON]    = { 1, 1, 10101202, "Z",        basic_seek },
	[BLUE_BAT]    = { 1, 1, 10101202, BLUE "B",   bat },
	[MONKEY]      = { 1, 0, 10004101, "M",        basic_seek },

	[BOMBER]      = { 1, 1, 99999998, RED "G",    diagonal_seek },
	[DIGGER]      = { 1, 1, 10101201, "G",        basic_seek },
	[BLACK_BAT]   = { 1, 0, 10401120, BLACK "B",  black_bat },
	[ARMADILDO]   = { 3, 0, 10303104, ORANGE "q", nop },
	[BLADENOVICE] = { 1, 1, 99999995, "b",        parry },
	[BLADEMASTER] = { 2, 1, 99999996, "b",        parry },
	[GHOUL]       = { 1, 0, 10301102, "W",        moore_seek },
	[OOZE_GOLEM]  = { 5, 3, 20510407, GREEN "'",  basic_seek },
	[HARPY]       = { 1, 1, 10301203, GREEN "h",  basic_seek },
	[LICH_1]      = { 1, 1, 10404202, "L",        basic_seek },
	[LICH_2]      = { 2, 1, 10404302, PURPLE "L", basic_seek },
	[LICH_3]      = { 3, 1, 10404402, BLACK "L",  basic_seek },
	[CONF_MONKEY] = { 1, 0, 10004103, GREEN "Y",  basic_seek },
	[TELE_MONKEY] = { 2, 0, 10002103, PURPLE "Y", basic_seek },
	[PIXIE]       = { 1, 0, 10401102, "n",        basic_seek },
	[SARCO_1]     = { 1, 9, 10101805, "|",        nop },
	[SARCO_2]     = { 2, 9, 10102910, YELLOW "|", nop },
	[SARCO_3]     = { 3, 9, 10103915, BLACK "|",  nop },
	[SPIDER]      = { 1, 1, 10401202, YELLOW "s", basic_seek },
	[WARLOCK_1]   = { 1, 1, 10401202, "w",        basic_seek },
	[WARLOCK_2]   = { 2, 1, 10401302, YELLOW "w", basic_seek },
	[MUMMY]       = { 1, 1, 30201103, "M",        moore_seek },
	[GARGOYLE_1]  = { 1, 1, 10401102, "g",        nop },
	[GARGOYLE_2]  = { 1, 1, 10401102, "g",        nop },
	[GARGOYLE_3]  = { 1, 1, 10401102, "g",        nop },
	[GARGOYLE_4]  = { 1, 1, 10401102, "g",        nop },
	[GARGOYLE_5]  = { 1, 1, 10401102, "g",        nop },
	[GARGOYLE_6]  = { 1, 1, 10401102, "g",        nop },

	[SHOPKEEPER]  = { 9, 9, 99999997, "@",        nop },
	[BLUE_DRAGON] = { 6, 1, 99999997, BLUE "D",   basic_seek },
	[MOMMY]       = { 6, 3, 30405215, BLACK "@",  basic_seek },

	[PLAYER]      = { 1, 0,      ~0u, "@",        NULL },
	[TRAP]        = { 1, 0,        1, "^",        spike_trap },
};
