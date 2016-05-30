// los.c - line-of-sight computations
// TODO: rename to utils.c, move other utilities here

#define IS_OPAQUE(y, x) (board[y][x].class == WALL)

// Checks whether the straight line from the player to the given coordinates
// is free from obstacles.
// This uses fractional coordinates: the center of tile (y, x) is at (y + 0.5, x + 0.5).
static bool los(double y, double x) {
	double dy = player.y - y;
	double dx = player.x - x;
	int cy = (int) (y + .5);
	int cx = (int) (x + .5);
	if ((player.x > x || x > cx) &&
		dy * (cy - y) > 0 &&
		IS_OPAQUE(cy, cx))
		return false;
	while (cy != player.y || cx != player.x) {
		double err_y = ABS((cx - x) * dy - (cy + SIGN(dy) - y) * dx);
		double err_x = ABS((cx + SIGN(dx) - x) * dy - (cy - y) * dx);
		int old_cx = cx;
		if (err_x < err_y + .001 && IS_OPAQUE(cy, cx += SIGN(dx)))
			return false;
		if (err_y < err_x + .001)
			if (IS_OPAQUE(cy += SIGN(dy), cx) || IS_OPAQUE(cy, old_cx))
				return false;
	}
	return true;
}

// Tests whether the player can see the tile at the given coordinates.
// This is true if there’s an unblocked line from the center of the player’s
// tile to any corner or the center of the destination tile.
static bool can_see(long y, long x) {
	if (y < player.y - 5 || y > player.y + 5 || x < player.x - 10 || x > player.x + 9)
		return false;
	return los(y - .55, x - .55)
		|| los(y - .55, x + .55)
		|| los(y + .55, x - .55)
		|| los(y + .55, x + .55)
		|| los(y, x);
}
