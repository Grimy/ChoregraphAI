static void basic_seek(Entity *this) {
	this->vertical =
		// #1: move towards the player
		dx == 0 ? 1 :
		dy == 0 ? 0 :

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

	move_enemy(this, this->vertical ? SIGN(dy) : 0, this->vertical ? 0 : SIGN(dx));
}

static void diagonal_seek(Entity *this) {
	move_enemy(
		this,
		dy ? SIGN(dy) : can_move(this, 1, SIGN(dx)) ? 1 : -1,
		dx ? SIGN(dx) : can_move(this, SIGN(dy), 1) ? 1 : -1
	);
}

static void bat(Entity *this) {
	static const int8_t bat_y[4] = {0, 0,  1, -1};
	static const int8_t bat_x[4] = {1, -1, 0, 0};
	int rng = rand();
	for (int i = 0; i < 4; ++i)
		if (move_enemy(this, bat_y[(rng + i) & 3], bat_x[(rng + i) & 3]))
			return;
}

static void black_bat(Entity *this) {
	if (ABS(dy) + ABS(dx) == 1)
		attack_player(this);
	else
		bat(this);
}

static void green_slime(Entity *this) {
	(void) this;
}

static Class class_infos[256] = {
	[PLAYER]      = { 1, 0, '@',  99999999, NULL },
	[DIRT]        = { 0, 0, '+',         0, NULL },
	[STONE]       = { 0, 0, ' ',         0, NULL },
	[TRAP]        = { 0, 0, '^',         1, green_slime },
	[SKELETON]    = { 1, 1, 'Z',  10101202, basic_seek },
	[BLUE_BAT]    = { 1, 1, 'B',  10101202, bat },
	[SPIDER]      = { 1, 1, 's',  10401202, basic_seek },
	[SHOPKEEPER]  = { 9, 9, '@',  99999997, green_slime },
	[BLUE_DRAGON] = { 6, 1, 'D',  99999997, basic_seek },
	[BOMBER]      = { 1, 1, 'G',  99999998, diagonal_seek },
	[DIGGER]      = { 1, 1, 'G',  10101201, basic_seek },
	[BLACK_BAT]   = { 1, 0, 'B',  10401120, black_bat },
	[BLADENOVICE] = { 1, 1, 'b',  99999995, basic_seek },
	[OOZE_GOLEM]  = { 5, 3, '\'', 20510407, basic_seek },
	[HARPY]       = { 1, 1, 'h',  10301203, basic_seek },
	[LICH_1]      = { 1, 1, 'L',  10404202, basic_seek },
	[CONF_MONKEY] = { 1, 0, 'M',  10004103, basic_seek },
	[TELE_MONKEY] = { 2, 0, 'M',  10004101, basic_seek },
	[SARCO_1]     = { 1, 9, '|',  10101805, green_slime },
	[WARLOCK_1]   = { 1, 1, 'w',         1, basic_seek },
	[GARGOYLE_1]  = { 1, 1, 'g',         1, basic_seek },
	[GARGOYLE_2]  = { 1, 1, 'g',         1, basic_seek },
	[GARGOYLE_3]  = { 1, 1, 'g',         1, basic_seek },
};
