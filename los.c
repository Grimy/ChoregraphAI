static int los(double y, double x) {
	double dy = player->y - y;
	double dx = player->x - x;
	int cy = (int) (y + .5);
	int cx = (int) (x + .5);
	if (dx * (cx - x) > 0 &&
		dy * (cy - y) > 0 &&
		has_wall(cy, cx))
		return 0;
	while (cy != player->y || cx != player->x) {
		double err_y = ABS((cx - x) * dy - (cy + SIGN(dy) - y) * dx);
		double err_x = ABS((cx + SIGN(dx) - x) * dy - (cy - y) * dx);
		int old_cx = cx;
		if (err_x < err_y + .001 && has_wall(cy, cx += SIGN(dx)))
			return 0;
		if (err_y < err_x + .001)
			if (has_wall(cy += SIGN(dy), cx) || has_wall(cy, old_cx))
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
