static int los(double y, double x) {
	double dy = player->y - y;
	double dx = player->x - x;
	int cy = (int) (y + .5);
	int cx = (int) (x + .5);
	double error = -(cy - y) * dx + (cx - x) * dy;
	while (cy != player->y || cx != player->x) {
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
	return 1;
}

static int can_see(int y, int x) {
	return los(y - .55, x - .55)
		|| los(y - .55, x + .55)
		|| los(y + .55, x - .55)
		|| los(y + .55, x + .55)
		|| los(y, x);
}
