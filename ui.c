static int has_wall(int y, int x) {
	if (y >= LENGTH(board) || x >= LENGTH(*board))
		return 0;
	for (Entity *e = board[y][x]; e; e = e->next)
		if (e->class == DIRT)
			return 1;
	return 0;
}

static int los(double y, double x) {
	double dy = y - player->y;
	double dx = x - player->x;
	for (double i = 0; i < 1; i += .01) {
		int ty = (int) (y - i * dy + .50);
		int tx = (int) (x - i * dx + .50);
		if (has_wall(ty, tx))
			return 0;
	}
	return 1;
}

static int can_see(int y, int x) {
	return los(y - .55, x - .55)
		|| los(y - .55, x + .55)
		|| los(y + .55, x - .55)
		|| los(y + .55, x + .55)
		|| los(y, x);
}

static void display_wall(int y, int x) {
	int glyph = has_wall(y - 1, x) << 3 |
		has_wall(y + 1, x) << 2 |
		has_wall(y, x - 1) << 1 |
		has_wall(y, x + 1);
	printf("%3.3s", &"╳───│┌┐┬│└┘┴│├┤┼"[3*glyph]);
}

static void display_board(void) {
	// printf("\033[H\033[2J");
	for (int y = 0; y < LENGTH(board); ++y) {
		for (int x = 0; x < LENGTH(*board); ++x) {
			Entity *e = board[y][x];
			if (!e)
				putchar('.');
			else if (e->class == DIRT)
				display_wall(y, x);
			else
				putchar(CLASS(e).glyph);
		}
		putchar('\n');
	}
}

static void player_input(Entity *this) {
	switch (getchar()) {
		case 'e': this->dy =  0; this->dx = -1; break;
		case 'f': this->dy =  1; this->dx =  0; break;
		case 'i': this->dy =  0; this->dx =  1; break;
		case 'j': this->dy = -1; this->dx =  0; break;
		case 't': this->hp = 0; break;
		default:  this->dy = 0; this->dx = 0; break;
	}
}
