static int los(double y, double x) {
	double dy = player->y - y;
	double dx = player->x - x;
	int cy = (int) (y + .5);
	int cx = (int) (x + .5);
	// double error = -(cy - y) * dx + (cx - x) * dy;
	double error = 0;
	if (dx * (cx - x) > 0 &&
		dy * (cy - y) > 0 &&
		has_wall(cy, cx))
		return 0;
	while (cy != player->y || cx != player->x) {
		double err_y = ABS(error - SIGN(dy) * dx);
		double err_x = ABS(error + SIGN(dx) * dy);
		int old_cx = cx;
		if (err_x < err_y + .001) {
			cx += SIGN(dx);
			if (has_wall(cy, cx))
				return 0;
			error += SIGN(dx) * dy;
		}
		if (err_y < err_x + .001) {
			cy += SIGN(dy);
			if (has_wall(cy, cx) || has_wall(cy, old_cx))
				return 0;
			error -= SIGN(dy) * dx;
		}
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
