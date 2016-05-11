
static void basic_seek(Entity *this) {
	if (this->state) {
		--this->state;
		return;
	}

	Entity *player = entities;
	int dy = player->y - this->y;
	int dx = player->x - this->x;
	int pdy = prev_y - this->y;
	int pdx = prev_x - this->x;

	int vertical =
		// #1: move towards the player
		dy == 0 ? 0 :
		dx == 0 ? 1 :

		// #2: avoid obstacles
		!can_move(this, SIGN(dy), 0) ? 0 :
		!can_move(this, 0, SIGN(dx)) ? 1 :
	
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
		ABS(dy) == 2 && dx == 2 ? player->y > SPAWN_Y :
		ABS(pdy) == 1 ? 0 :
		ABS(pdx) == 1 ? 1 :

		// #6: keep moving along the same axis
		!!this->dy;

	this->dy = vertical ? SIGN(dy) : 0;
	this->dx = vertical ? 0 : SIGN(dx);

	if (can_move(this, this->dy, this->dx)) {
		move_ent(this);
		this->state = CLASS(this).beat_delay;
	}
}

static void green_slime(Entity *this) {
	(void) this;
}

static Class class_infos[256] = {
	[PLAYER]      = { 1, 0, '@', 9999, player_input },
	[SKELETON]    = { 1, 1, 'S', 1001, basic_seek },
	[BLACK_BAT]   = { 1, 1, 'B',    1, basic_seek },
	[SPIDER]      = { 1, 1, 's',    1, basic_seek },
	[SHOPKEEPER]  = { 1, 9, '@',    1, green_slime },
	[BLUE_DRAGON] = { 1, 1, 'D',    1, basic_seek },
	[DIRT]        = { 0, 0, '+',    0, NULL },
	[STONE]       = { 0, 0, ' ',    0, NULL },
	[TRAP]        = { 0, 0, '^',    1, green_slime },
	[9]           = { 1, 1, 'R',    1, basic_seek },
	[44]          = { 1, 1, 'Q',    1, basic_seek },
	[45]          = { 1, 1, 'A',    1, basic_seek },
	[46]          = { 1, 1, 'C',    1, basic_seek },
	[48]          = { 1, 1, 'P',    1, basic_seek },
	[51]          = { 1, 1, 'E',    1, basic_seek },
	[52]          = { 1, 1, 'F',    1, basic_seek },
	[53]          = { 1, 1, 'G',    1, basic_seek },
	[56]          = { 1, 1, 'H',    1, basic_seek },
	[57]          = { 1, 1, 'I',    1, basic_seek },
	[59]          = { 1, 1, 'J',    1, basic_seek },
	[63]          = { 1, 1, 'L',    1, basic_seek },
	[66]          = { 1, 1, 'M',    1, basic_seek },
	[67]          = { 1, 1, 'N',    1, basic_seek },
	[68]          = { 1, 1, 'O',    1, basic_seek },
};
