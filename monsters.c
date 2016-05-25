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

		// #4: if prevpos aligns with the player’s curpos or prevpos,
		// move into the direction of the alignment (modulo a bug)
		this->prev_y == player->y && dx < 0 ? 0 :
		this->prev_x == player->x ? 1 :
		this->prev_y == player->y ? 0 :
		this->prev_y == player->prev_y ? dx > 0 && player->x > SPAWN_X :
		this->prev_x == player->prev_x ? 1 :

		// #5: keep moving along the same axis
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
	[SKELETON]    = { 1, 1, 'Z',  10101202, basic_seek },
	[BLUE_BAT]    = { 1, 1, 'B',  10101202, bat },
	[MONKEY]      = { 1, 0, 'M',  10004101, basic_seek },

	[BOMBER]      = { 1, 1, 'G',  99999998, diagonal_seek },
	[DIGGER]      = { 1, 1, 'G',  10101201, basic_seek },
	[BLACK_BAT]   = { 1, 0, 'B',  10401120, black_bat },
	[ARMADILDO]   = { 3, 0, 'q',  10303104, nop },
	[BLADENOVICE] = { 1, 1, 'b',  99999995, parry },
	[BLADEMASTER] = { 2, 1, 'b',  99999996, parry },
	[GHOUL]       = { 1, 0, 'W',  10301102, moore_seek },
	[OOZE_GOLEM]  = { 5, 3, '\'', 20510407, basic_seek },
	[HARPY]       = { 1, 1, 'h',  10301203, basic_seek },
	[LICH_1]      = { 1, 1, 'L',  10404202, basic_seek },
	[LICH_2]      = { 2, 1, 'L',  10404302, basic_seek },
	[LICH_3]      = { 3, 1, 'L',  10404402, basic_seek },
	[CONF_MONKEY] = { 1, 0, 'Y',  10004103, basic_seek },
	[TELE_MONKEY] = { 2, 0, 'Y',  10002103, basic_seek },
	[PIXIE]       = { 1, 0, 'n',  10401102, basic_seek },
	[SARCO_1]     = { 1, 9, '|',  10101805, nop },
	[SARCO_2]     = { 2, 9, '|',  10102910, nop },
	[SARCO_3]     = { 3, 9, '|',  10103915, nop },
	[SPIDER]      = { 1, 1, 's',  10401202, basic_seek },
	[WARLOCK_1]   = { 1, 1, 'w',  10401202, basic_seek },
	[WARLOCK_2]   = { 2, 1, 'w',  10401302, basic_seek },
	[MUMMY]       = { 1, 1, 'M',  30201103, moore_seek },
	[GARGOYLE_1]  = { 1, 1, 'g',  10401102, nop },
	[GARGOYLE_2]  = { 1, 1, 'g',  10401102, nop },
	[GARGOYLE_3]  = { 1, 1, 'g',  10401102, nop },
	[GARGOYLE_4]  = { 1, 1, 'g',  10401102, nop },
	[GARGOYLE_5]  = { 1, 1, 'g',  10401102, nop },
	[GARGOYLE_6]  = { 1, 1, 'g',  10401102, nop },

	[SHOPKEEPER]  = { 9, 9, '@',  99999997, nop },
	[BLUE_DRAGON] = { 6, 1, 'D',  99999997, basic_seek },
	[MOMMY]       = { 6, 3, '@',  30405215, basic_seek },

	[PLAYER]      = { 1, 0, '@',       ~0u, NULL },
	[TRAP]        = { 1, 0, '^',         1, spike_trap },
};
