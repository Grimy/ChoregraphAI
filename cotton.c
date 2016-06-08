// cotton.c - core game logic

#define LENGTH(array) ((long) (sizeof(array) / sizeof(*(array))))
#define SIGN(x) (((x) > 0) - ((x) < 0))
#define DIRECTION(pos) ((Coords) {SIGN((pos).x), SIGN((pos).y)})
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define L1(pos) (ABS((pos).x) + ABS((pos).y))
#define L2(pos) ((pos).x * (pos).x + (pos).y * (pos).y)

#define TILE(pos) (board[(pos).y][(pos).x])
#define CLASS(m) (class_infos[(m)->class])

#define IS_ENEMY(m) ((m) && (m) != &player)
#define IS_OPAQUE(x, y) (board[y][x].class == WALL)
#define IS_MIMIC(c) ((c) == TARMONSTER || (c) == WALL_MIMIC || (c) == SEEK_STATUE \
		|| (c) == FIRE_MIMIC || (c) == ICE_MIMIC)
#define IS_KNOCKED_BACK(c) ((c) == MONKEY_2 || (c) == TELE_MONKEY \
		|| (c) == ASSASSIN_2 || (c) == BANSHEE_1 || (c) == BANSHEE_2)
#define BLOCKS_MOVEMENT(pos) (TILE(pos).class == WALL)

static void damage(Monster *m, long dmg, bool bomblike);

// Moves the given monster to a specific position.
// Keeps track of the monster’s previous position.
static void move(Monster *m, Coords dest) {
	TILE(m->pos).monster = NULL;
	m->untrapped = false;
	m->prev_pos = m->pos;
	m->pos = dest;
	TILE(m->pos).monster = m;
}

// Tests whether the given monster can move in the given direction.
// The code assumes that only spiders can be inside walls. This will need to
// change before adding phasing enemies.
static bool can_move(Monster *m, Coords offset) {
	Tile dest = TILE(m->pos + offset);
	if (dest.monster)
		return dest.monster == &player;
	if (TILE(m->pos).class == WALL)
		return dest.class == WALL && !dest.torch;
	return dest.class != WALL;
}

// Tries to dig away the given wall, replacing it with floor.
// Returns whether the dig succeeded.
static bool dig(Tile *wall, int digging_power, bool z4) {
	if (wall->class != WALL || wall->hp > digging_power)
		return false;
	if (z4 && (wall->hp == 0 || wall->hp > 2))
		return false;
	wall->class = FLOOR;
	if (wall->monster && wall->monster->class == SPIDER) {
		wall->monster->class = FREE_SPIDER;
		wall->monster->delay = 1;
	}
	if (!z4 && wall->zone == 4 && (wall->hp == 1 || wall->hp == 2)) {
		dig(wall - 1, digging_power, true);
		dig(wall + 1, digging_power, true);
		dig(wall - LENGTH(*board), digging_power, true);
		dig(wall + LENGTH(*board), digging_power, true);
	}
	return true;
}

// Removes a monster from both the board and the priority queue.
static void monster_remove(Monster *m) {
	if (m == &player)
		exit(1);
	TILE(m->pos).monster = NULL;
	Monster *prev = &player;
	while (prev->next != m)
		prev = prev->next;
	prev->next = m->next;
}

// Handles an enemy attacking the player.
// Usually boils down to `damage(&player, ...)`, but some enemies are special-cased.
static void enemy_attack(Monster *attacker) {
	if (attacker->class == CONF_MONKEY) {
		monster_remove(attacker);
		player.confusion = 5;
	} else if (attacker->class == PIXIE) {
		monster_remove(attacker);
	} else {
		damage(&player, 1, false);
	}
}

static bool before_move(Monster *m) {
	if (m->freeze)
		return false;
	if (TILE(m->pos).class == WATER && !CLASS(m).flying) {
		TILE(m->pos).class = FLOOR;
		return false;
	}
	if (TILE(m->pos).class == TAR && !CLASS(m).flying && !m->untrapped) {
		m->untrapped = true;
		return false;
	}
	return true;
}

// Attempts to move the given monster by the given offset.
// Will trigger attacking/digging if the destination contains the player/a wall.
// On success, resets the enemy’s delay and returns true.
static bool enemy_move(Monster *m, Coords offset) {
	m->delay = CLASS(m).beat_delay;
	if (!before_move(m))
		return true;
	if (m->confusion)
		offset = -offset;

	Tile *dest = &TILE(m->pos + offset);
	if (dest->monster == &player)
		enemy_attack(m);
	else if (can_move(m, offset))
		move(m, m->pos + offset);
	else if (dig(dest, m->confusion ? -1 : CLASS(m).dig, false))
		return false;
	else
		return m->delay = 0;

	return true;
}

// Moves something by force (as caused by bounce traps, wind mages and knockback).
// Unlike enemy_move, ignores confusion, delay, and digging.
static void forced_move(Monster *m, Coords offset) {
	if (!before_move(m))
		return;
	Tile *dest = &TILE(m->pos + offset);
	if (dest->monster == &player)
		enemy_attack(m);
	else if (!dest->monster && dest->class != WALL)
		move(m, m->pos + offset);
}

// Checks whether the straight line from the player to the given position
// is free from obstacles.
// Uses fractional coordinates: the center of tile (y, x) is at (y + 0.5, x + 0.5).
static bool los(double x, double y) {
	double dx = player.pos.x - x;
	double dy = player.pos.y - y;
	int cx = (int) (x + .5);
	int cy = (int) (y + .5);
	if ((player.pos.x > x || x > cx) && dy * (cy - y) > 0 && IS_OPAQUE(cx, cy))
		return false;
	while (cx != player.pos.x || cy != player.pos.y) {
		double err_x = ABS((cx + SIGN(dx) - x) * dy - (cy - y) * dx);
		double err_y = ABS((cx - x) * dy - (cy + SIGN(dy) - y) * dx);
		if ((ABS(err_x - err_y) < .001 && IS_OPAQUE(cx, cy + SIGN(dy)))
			|| (err_x < err_y + .001 && IS_OPAQUE(cx += SIGN(dx), cy))
			|| (err_y < err_x + .001 && IS_OPAQUE(cx, cy += SIGN(dy))))
			return false;
	}
	return true;
}

// Tests whether the player can see the tile at the given position.
// This is true if there’s an unblocked line from the center of the player’s
// tile to any corner or the center of the destination tile.
static bool can_see(Coords dest) {
	Coords pos = player.pos;
	if (dest.x < pos.x - 10 || dest.x > pos.x + 9 || dest.y < pos.y - 5 || dest.y > pos.y + 5)
		return false;
	return los(dest.x - .55, dest.y - .55)
		|| los(dest.x + .55, dest.y - .55)
		|| los(dest.x - .55, dest.y + .55)
		|| los(dest.x + .55, dest.y + .55)
		|| los(dest.x, dest.y);
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
	forced_move(m, DIRECTION(m->pos - player.pos));
	m->delay = 1;
}

// Places a bomb at the given position.
static void bomb_plant(Coords pos, uint8_t delay) {
	Monster bomb = {.class = BOMB, .pos = pos, .next = player.next, .aggro = true, .delay = delay};
	player.next = &monsters[monster_count];
	monsters[monster_count++] = bomb;
}

static void bomb_tile(Tile *tile) {
	if (tile->monster)
		damage(tile->monster, 4, true);
	if ((tile->class == WALL && tile->hp < 5) || tile->class == WATER)
		tile->class = FLOOR;
	else if (tile->class == ICE)
		tile->class = WATER;
}

static void bomb_tick(Monster *this, __attribute__((unused)) Coords d) {
	monster_remove(this);
	for (int x = this->pos.x - 1; x <= this->pos.x + 1; ++x)
		for (int y = this->pos.y - 1; y <= this->pos.y + 1; ++y)
			bomb_tile(&board[y][x]);
}

static void tile_change(Tile *tile, TileClass new_class) {
	tile->class =
		tile->class == STAIRS ? STAIRS :
		tile->class * new_class == FIRE * ICE ? WATER :
		tile->class == WATER && new_class == FIRE ? FLOOR :
		new_class;
}

// Kills the given monster, handling any on-death effects.
static void kill(Monster *m, bool bomblike) {
	monster_remove(m);
	Tile *tile = &TILE(m->pos);
	if (!bomblike && (m->class == WARLOCK_1 || m->class == WARLOCK_2))
		move(&player, m->pos);
	else if (m->class == ICE_SLIME || m->class == YETI)
		tile_change(tile, ICE);
	else if (m->class == FIRE_SLIME || m->class == HELLHOUND)
		tile_change(tile, FIRE);
	else if (m->class == BOMBER)
		bomb_plant(m->pos, 3);
}

// Deals damage to the given monster.
static void damage(Monster *m, long dmg, bool bomblike) {
	if (m->class == WIND_STATUE) {
		knockback(m);
	} else if (m->class == MINE_STATUE) {
		bomb_tick(m, spawn);
	} else if (m->class == BOMB_STATUE) {
		knockback(m);
		m->delay = 2;
	} else if (dmg < 3 && (m->class == CRATE_1 || m->class == CRATE_2)) {
		knockback(m);
	} else if (dmg == 0) {
		return;
	} else if (m->class == PIXIE) {
		bomb_tick(m, spawn);
	} else if (!bomblike && (m->class == BLADENOVICE || m->class == BLADEMASTER) && m->state < 2) {
		knockback(m);
		m->state = 1;
	} else if (m->class >= RIDER_1 && m->class <= RIDER_3) {
		knockback(m);
		m->class += SKELETANK_1 - RIDER_1;
	} else if ((m->class == ARMADILLO_1 || m->class == ARMADILLO_2 || m->class == ARMADILDO) && m->state) {
		m->prev_pos = player.pos;
	} else if (IS_MIMIC(m->class) && m->state < 2) {
		return;
	} else if (m->class == MOLE && m->state == 0) {
		return;
	} else {
		m->hp -= dmg;
	}

	if (m->class == OOZE_GOLEM)
		tile_change(&TILE(player.pos), OOZE);

	if (m->hp <= 0)
		kill(m, bomblike);
	else if (IS_KNOCKED_BACK(m->class))
		knockback(m);
}

// Attempts to move the player by the given offset.
// Will trigger attacking/digging if the destination contains an enemy/a wall.
static void player_move(Coords offset) {
	if (!before_move(&player))
		return;
	if (player.confusion)
		offset = -offset;
	Tile *dest = &TILE(player.pos + offset);
	player.prev_pos = player.pos;
	if (dest->class == WALL)
		dig(dest, TILE(player.pos).class == OOZE ? 0 : 2, false);
	else if (IS_ENEMY(dest->monster))
		damage(dest->monster, TILE(player.pos).class == OOZE ? 0 : 1, false);
	else
		move(&player, player.pos + offset);
}

// Deals bomb-like damage to all monsters on a horizontal line).
static void fireball(Coords pos, int8_t dir) {
	for (Tile *tile = &TILE(pos) + dir; tile->class != WALL; tile += dir)
		if (tile->monster)
			damage(tile->monster, 5, true);
}

// Changes water to ice and freezes monsters on the given tile.
static void freeze(Tile *tile) {
	if (tile->class == WATER)
		tile->class = ICE;
	if (tile->monster)
		tile->monster->freeze = 5;
}

// Freezes each tile in a 3x5 cone.
static void cone_of_cold(Coords pos, int8_t dir) {
	for (int8_t x = 1; x <= 3; ++x)
		for (int8_t y = 1 - x; y < x; ++y)
			freeze(&TILE(pos + ((Coords) {x * dir, y})));
}
