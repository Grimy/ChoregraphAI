static void display_wall(int y, int x) {
	int glyph = has_wall(y - 1, x) << 3 |
		has_wall(y + 1, x) << 2 |
		has_wall(y, x - 1) << 1 |
		has_wall(y, x + 1);
	printf("%3.3s", &"╳╳╳─╳┌┐┬╳└┘┴│├┤┼"[3*glyph]);
}

static void display(void) {
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
	display();
	switch (getchar()) {
		case 'e': this->dy =  0; this->dx = -1; break;
		case 'f': this->dy =  1; this->dx =  0; break;
		case 'i': this->dy =  0; this->dx =  1; break;
		case 'j': this->dy = -1; this->dx =  0; break;
		case 't': this->hp = 0; return;
		default: return;
	}
	if (can_move(this, this->dy, this->dx)) {
		prev_y = this->y;
		prev_x = this->x;
		move_ent(this);
	}
}
