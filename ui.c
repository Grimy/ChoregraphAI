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
				can_see(y, x) ? display_wall(y, x) : (void) putchar(' ');
			else
				putchar(CLASS(e).glyph);
		}
		putchar('\n');
	}
}

static Point player_input(Entity *this) {
	switch (getchar()) {
		case 'e': return (Point) { 0, -1}; break;
		case 'f': return (Point) { 1,  0}; break;
		case 'i': return (Point) { 0,  1}; break;
		case 'j': return (Point) {-1,  0}; break;
		case 't': this->hp = 0; break;
	}
	return (Point) {0, 0};
}
