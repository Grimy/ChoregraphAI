static int has_wall(int y, int x) {
	if (y >= LENGTH(board) || x >= LENGTH(*board))
		return 0;
	for (Entity *e = board[y][x]; e; e = e->next)
		if (e->class == DIRT)
			return 1;
	return 0;
}

static int los(double y, double x) {
	// printf("Player: %d %d\n", player->y, player->x);
	// printf("Destination: %f %f\n", y, x);
	double dy = player->y - y;
	double dx = player->x - x;
	int cy = (int) (y + .5);
	int cx = (int) (x + .5);
	double error = -(cy - y) * dx + (cx - x) * dy;
	while (cy != player->y || cx != player->x) {
		// printf("%f %d %d\n", error, cy, cx);
		if (has_wall(cy, cx))
			return 0;
		if (SIGN(dx) == 0) {
			cy += SIGN(dy);
			continue;
		}
		if (SIGN(dy) == 0) {
			cx += SIGN(dx);
			continue;
		}
		double err_y = ABS(error - SIGN(dy) * dx);
		double err_x = ABS(error + SIGN(dx) * dy);
		// printf("errors: %f %f %d\n", err_y, err_x, ABS(err_y - err_x) < .001);
		if (ABS(err_y - err_x) < .001) {
			if (has_wall(cy + SIGN(dy), cx) || has_wall(cy, cx + SIGN(dx)))
				return 0;
			cy += SIGN(dy);
			cx += SIGN(dx);
			error += SIGN(dx) * dy - SIGN(dy) * dx;
		} else if (err_x < err_y) {
			cx += SIGN(dx);
			error += SIGN(dx) * dy;
		} else {
			cy += SIGN(dy);
			error -= SIGN(dy) * dx;
		}
	}
	// printf("Tile is visible!\n");
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
	printf("\033[H\033[2J");
	for (int y = 0; y < LENGTH(board); ++y) {
		for (int x = 0; x < LENGTH(*board); ++x) {
			Entity *e = board[y][x];
			if (!e)
				putchar(can_see(y, x) ? '.' : ' ');
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
