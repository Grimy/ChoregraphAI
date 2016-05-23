static int is_free(Entity *e) {
	return e == NULL;
}

static void move_ent(Entity *e) {
	rm_ent(e);
	e->y += e->dy;
	e->x += e->dx;
	add_ent(e);
}

static void attack_player(Entity *attacker) {
	if (attacker->class == CONF_MONKEY) {
		attacker->hp = 0;
		rm_ent(attacker);
	} else {
		player->hp = 0;
	}
}

static int move_enemy(Entity *e) {
	Entity *dest = board[e->y + e->dy][e->x + e->dx];
	if (dest && dest->class == PLAYER) {
		attack_player(e);
		return 1;
	}
	if (!is_free(dest))
		return 0;
	move_ent(e);
	return 1;
}

static void player_turn(Entity *this) {
	display_board();
	player_input(this);
	
	Entity *dest = board[this->y + this->dy][this->x + this->dx];

	if (dest == NULL) {
		prev_y = this->y;
		prev_x = this->x;
		move_ent(this);
	} else if (dest->class < PLAYER) {
		dest->hp -= 1;
		if (dest->hp <= 0)
			rm_ent(dest);
	} else if (dest->class == DIRT) {
		board[this->y + this->dy][this->x + this->dx] = NULL;
	}
}

static void basic_seek(Entity *this) {
	int dy = player->y - this->y;
	int dx = player->x - this->x;
	int pdy = prev_y - this->y;
	int pdx = prev_x - this->x;

	int vertical =
		// #1: move towards the player
		dx == 0 ? 1 :
		dy == 0 ? 0 :

		// #2: avoid obstacles
		!is_free(board[this->y + SIGN(dy)][this->x]) ? 0 :
		!is_free(board[this->y][this->x + SIGN(dx)]) ? 1 :
	
		// #3: move towards the player’s previous position
		pdy == 0 ? 0 :
		pdx == 0 ? 1 :

		// #4: keep moving in the same direction
		this->dx * dx > 0 ? 0 :
		this->dy * dy > 0 ? 1 :

		// #5: special cases
		ABS(dy) == 1 && dx < 0 ? 0 :
		ABS(dx) == 1 ? 1 :
		ABS(dy) == 1 ? 0 :
		ABS(dy) == 2 && dx == -2 ? 0 :
		ABS(dy) == 2 && dx == 2 ? player->x > SPAWN_X :
		ABS(pdy) == 1 ? 0 :
		ABS(pdx) == 1 ? 1 :

		// #6: keep moving along the same axis
		!!this->dy;

	this->dy = vertical ? SIGN(dy) : 0;
	this->dx = vertical ? 0 : SIGN(dx);

	if (move_enemy(this))
		this->delay = CLASS(this).beat_delay;
}

static void diagonal_seek(Entity *this) {
	int dy = player->y - this->y;
	int dx = player->x - this->x;
	this->dy = dy ? SIGN(dy) : 1;
	this->dx = dx ? SIGN(dx) : 1;
	// TODO add obstacle avoiding logic
}

static void bat(Entity *this) {
	int rng = rand();
	for (int i = 0; i < 4; ++i) {
		this->dy = (int[]) {0, 0, 1, -1} [(rng + i) & 3];
		this->dx = (int[]) {1, -1, 0, 0} [(rng + i) & 3];
		if (move_enemy(this))
			break;
	}
}

static void black_bat(Entity *this) {
	int dy = player->y - this->y;
	int dx = player->x - this->x;
	if (dy * dy + dx * dx == 1)
		attack_player(this);
	else
		bat(this);
}

static void green_slime(Entity *this) {
	(void) this;
}

static Class class_infos[256] = {
	[PLAYER]      = { 1, 0, '@',  99999999, player_turn },
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
