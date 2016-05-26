#define BLOCKS_LOS(y, x) (board[y][x].class == WALL)

static int los(double y, double x) {
	double dy = player->y - y;
	double dx = player->x - x;
	int cy = (int) (y + .5);
	int cx = (int) (x + .5);
	if ((player->x > x || x > cx) &&
		dy * (cy - y) > 0 &&
		BLOCKS_LOS(cy, cx))
		return 0;
	while (cy != player->y || cx != player->x) {
		double err_y = ABS((cx - x) * dy - (cy + SIGN(dy) - y) * dx);
		double err_x = ABS((cx + SIGN(dx) - x) * dy - (cy - y) * dx);
		int old_cx = cx;
		if (err_x < err_y + .001 && BLOCKS_LOS(cy, cx += SIGN(dx)))
			return 0;
		if (err_y < err_x + .001)
			if (BLOCKS_LOS(cy += SIGN(dy), cx) || BLOCKS_LOS(cy, old_cx))
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
