// cotton.c - core game logic

#define LENGTH(array) ((long) (sizeof(array) / sizeof(*(array))))
#define SIGN(x) (((x) > 0) - ((x) < 0))
#define ABS(x)  ((x) < 0 ? -(x) : (x))

#define IS_ENEMY(m) ((m) && (m) != &player)
#define IS_OPAQUE(y, x) (board[y][x].class == WALL)
#define CLASS(m) (class_infos[(m)->class])

// Moves a monster from one tile to another.
// This also updates the monster’s current and previous positions.
static void ent_move(Monster *m, int8_t y, int8_t x) {
	board[m->y][m->x].next = NULL;
	m->prev_y = m->y;
	m->prev_x = m->x;
	m->y = y;
	m->x = x;
	board[m->y][m->x].next = m;
}

// Tests whether the given monster can move in the given direction.
// The code assumes that only spiders can be inside walls. This will need to
// change before adding phasing enemies.
static bool can_move(Monster *m, long dy, long dx) {
	Tile dest = board[m->y + dy][m->x + dx];
	if (IS_ENEMY(dest.next) || dest.torch)
		return false;
	return (board[m->y][m->x].class == WALL) == (dest.class == WALL);
}

static void monster_attack(Monster *attacker) {
	if (attacker->class == CONF_MONKEY || attacker->class == PIXIE) {
		attacker->hp = 0;
		board[attacker->y][attacker->x].next = NULL;
	} else {
		player.hp = 0;
	}
}

// Performs a movement action on behalf of an enemy.
// This includes attacking the player if they block the movement.
// TODO: implement enemy digging
static bool monster_move(Monster *m, int8_t dy, int8_t dx) {
	if (!(can_move(m, dy, dx)))
		return false;
	m->delay = CLASS(m).beat_delay;
	Tile dest = board[m->y + dy][m->x + dx];
	if (dest.next == &player)
		monster_attack(m);
	else
		ent_move(m, m->y + dy, m->x + dx);
	return true;
}

// Moves something by force, as caused by bounce traps, wind mages and knockback.
// Unlike monster_move, this ignores confusion, doesn’t change the beat delay,
// and doesn’t cause digging.
static void forced_move(Monster *m, int8_t dy, int8_t dx) {
	Tile dest = board[m->y + dy][m->x + dx];
	if (dest.next == &player)
		monster_attack(m);
	else if (dest.class != WALL)
		ent_move(m, m->y + dy, m->x + dx);
}

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

// Compares the priorities of two monsters.
// Meant to be used as a callback for qsort.
static int compare_priorities(const void *a, const void *b) {
	uint32_t pa = CLASS((const Monster*) a).priority;
	uint32_t pb = CLASS((const Monster*) b).priority;
	return (pb > pa) - (pb < pa);
}

// Knocks an enemy away from the player.
// TODO: set the knockback direction correctly for diagonal attacks.
static void knockback(Monster *m) {
	forced_move(m, SIGN(m->y - player.y), SIGN(m->x - player.x));
	m->delay = 1;
}

// Deals damage to the given monster.
static void damage(Monster *m, long dmg, bool bomblike) {
	if (!bomblike && (m->class == BLADENOVICE || m->class == BLADEMASTER) && m->state < 2) {
		knockback(m);
		m->state = 1;
		return;
	}
	m->hp -= dmg;
	if (m->hp > 0)
		return;
	board[m->y][m->x].next = NULL;
	if (!bomblike && (m->class == WARLOCK_1 || m->class == WARLOCK_2))
		ent_move(&player, m->y, m->x);
}

static void player_attack(Monster *m) {
	if (board[player.y][player.x].class == OOZE)
		return;
	if (m->class == OOZE_GOLEM)
		board[player.y][player.x].class = OOZE;
	damage(m, 1, false);
	if (m->hp > 0 && CLASS(m).beat_delay == 0)
		knockback(m);
}

static void zone4_dig(Tile *tile) {
	if (tile->hp == 1 || tile->hp == 2)
		tile->class = FLOOR;
}

static void player_dig(Tile *wall) {
	long dig = board[player.y][player.x].class == OOZE ? 0 : 2;
	if (dig < wall->hp)
		return;
	wall->class = FLOOR;
	if (wall->zone == 4 && (wall->hp == 1 || wall->hp == 2)) {
		zone4_dig(wall - 1);
		zone4_dig(wall + 1);
		zone4_dig(wall - LENGTH(*board));
		zone4_dig(wall + LENGTH(*board));
	}
}

static void player_move(int8_t y, int8_t x) {
	Tile *dest = &board[player.y + y][player.x + x];
	player.prev_y = player.y;
	player.prev_x = player.x;
	if (dest->class == WALL)
		player_dig(dest);
	else if (IS_ENEMY(dest->next))
		player_attack(dest->next);
	else
		ent_move(&player, player.y + y, player.x + x);
}
