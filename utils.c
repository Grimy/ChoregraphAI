// utils.c - core game logic

#define DIRECTION(pos) ((Coords) {SIGN((pos).x), SIGN((pos).y)})
#define L1(pos) (ABS((pos).x) + ABS((pos).y))
#define L2(pos) ((pos).x * (pos).x + (pos).y * (pos).y)

#define TILE(pos) (board[(pos).x][(pos).y])
#define CLASS(m) (class_infos[(m)->class])

#define IS_ENEMY(m) ((m) && (m) != &player)
#define IS_OPAQUE(x, y) (board[x][y].class == WALL)
#define IS_MIMIC(c) ((c) == TARMONSTER || (c) == WALL_MIMIC || (c) == SEEK_STATUE \
		|| (c) == FIRE_MIMIC || (c) == ICE_MIMIC)
#define IS_KNOCKED_BACK(c) ((c) == MONKEY_2 || (c) == TELE_MONKEY \
		|| (c) == ASSASSIN_2 || (c) == BANSHEE_1 || (c) == BANSHEE_2)
#define BLOCKS_MOVEMENT(pos) (TILE(pos).class == WALL)

static const i64 plus_shape[] = {-32, -1, 1, 32};
static const i64 cone_shape[] = {32, 63, 64, 65, 94, 95, 96, 97, 98};
static const i64 square_shape[] = {
	-66, -65, -64, -63, -62, -34, -33, -32, -31, -30, -2, -1,
	0, 1, 2, 30, 31, 32, 33, 34, 62, 63, 64, 65, 66
};

// Moves the given monster to a specific position.
// Keeps track of the monster’s previous position.
static void move(Monster *m, Coords dest)
{
	TILE(m->pos).monster = NULL;
	m->untrapped = false;
	m->pos = dest;
	TILE(m->pos).monster = m;
}

// Tests whether the given monster can move in the given direction.
// The code assumes that only spiders can be inside walls. This will need to
// change before adding phasing enemies.
static bool can_move(Monster *m, Coords offset)
{
	assert(m != &player || offset.x || offset.y);
	Tile dest = TILE(m->pos + offset);
	if (dest.monster)
		return dest.monster == &player;
	if (TILE(m->pos).class == WALL)
		return dest.class == WALL && !dest.torch;
	return dest.class != WALL;
}

static void adjust_lights(Tile *tile, i8 diff) {
	for (i64 i = 0; i < LENGTH(square_shape); ++i)
		(tile + square_shape[i])->light += diff;
}

// Tries to dig away the given wall, replacing it with floor.
// Returns whether the dig succeeded.
static bool dig(Tile *wall, i64 digging_power, bool z4)
{
	if (wall->class != WALL || wall->hp > digging_power)
		return false;
	if (z4 && (wall->hp == 0 || wall->hp > 2))
		return false;
	wall->class =
		wall->hp == 2 && wall->zone == 2 ? FIRE :
		wall->hp == 2 && wall->zone == 3 ? ICE :
		FLOOR;
	if (wall->monster && wall->monster->class == SPIDER) {
		wall->monster->class = FREE_SPIDER;
		wall->monster->delay = 1;
	}
	if (wall->torch)
		adjust_lights(wall, -1);
	if (!z4 && wall->zone == 4 && (wall->hp == 1 || wall->hp == 2))
		for (i64 i = 0; i < LENGTH(plus_shape); ++i)
			dig(wall + plus_shape[i], digging_power, true);
	return true;
}

// Removes a monster from the priority queue.
static void monster_remove(Monster *m)
{
	Monster *prev = &player;
	while (prev->next != m)
		prev = prev->next;
	prev->next = m->next;
}

// Handles an enemy attacking the player.
// Usually boils down to `damage(&player, ...)`, but some enemies are special-cased.
static void enemy_attack(Monster *attacker)
{
	Coords d = player.pos - attacker->pos;
	switch (attacker->class) {
	case CONF_MONKEY:
		player.confusion = 2;
		// FALLTHROUGH
	case PIXIE:
		TILE(attacker->pos).monster = NULL;
		monster_remove(attacker);
		break;
	case SHOVE_1:
	case SHOVE_2:
		if (forced_move(&player, d))
			move(attacker, attacker->pos + d);
		else
			damage(&player, 1, false);
		break;
	default:
		damage(&player, 1, false);
	}
}

static bool before_move(Monster *m)
{
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
static bool enemy_move(Monster *m, Coords offset)
{
	m->prev_pos = m->pos;
	m->delay = CLASS(m).beat_delay;
	if (!before_move(m))
		return false;
	if (m->confusion)
		offset = -offset;

	Tile *dest = &TILE(m->pos + offset);
	if (dest->monster == &player) {
		enemy_attack(m);
	} else if (can_move(m, offset)) {
		move(m, m->pos + offset);
		return true;
	} else if (!dig(dest, m->confusion ? -1 : CLASS(m).dig, false)) {
		m->delay = 0;
	}

	return false;
}

// Moves something by force (as caused by bounce traps, wind mages and knockback).
// Unlike enemy_move, ignores confusion, delay, and digging.
static bool forced_move(Monster *m, Coords offset)
{
	assert(offset.x || offset.y);
	if (!before_move(m))
		return false;
	Tile *dest = &TILE(m->pos + offset);
	if (dest->monster == &player) {
		enemy_attack(m);
		return true;
	} else if (!dest->monster && dest->class != WALL) {
		m->prev_pos = m->pos;
		move(m, m->pos + offset);
		return true;
	}
	return false;
}

// Checks whether the straight line from the player to the given position
// is free from obstacles.
// Uses fractional coordinates: the center of tile (y, x) is at (y + 0.5, x + 0.5).
static bool los(double x, double y)
{
	double dx = player.pos.x - x;
	double dy = player.pos.y - y;
	i64 cx = (i64) (x + .5);
	i64 cy = (i64) (y + .5);
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
static bool can_see(Coords dest)
{
	Coords pos = player.pos;
	if (dest.x < pos.x - 10 || dest.x > pos.x + 9 || dest.y < pos.y - 5 || dest.y > pos.y + 5)
		return false;

	// Miner’s Cap
	if (L2(dest - pos) <= 9)
		return true;

	if (!TILE(dest).light)
		return false;

	return los(dest.x - .55, dest.y - .55)
	    || los(dest.x + .55, dest.y - .55)
	    || los(dest.x - .55, dest.y + .55)
	    || los(dest.x + .55, dest.y + .55)
	    || los(dest.x, dest.y);
}

// Compares the priorities of two monsters.
// Meant to be used as a callback for qsort.
static i32 compare_priorities(const void *a, const void *b)
{
	u32 pa = CLASS((const Monster*) a).priority;
	u32 pb = CLASS((const Monster*) b).priority;
	return (pb > pa) - (pb < pa);
}

// Knocks an enemy away from the player.
// TODO: set the knockback direction correctly for diagonal attacks.
static void knockback(Monster *m)
{
	forced_move(m, DIRECTION(m->pos - player.pos));
	m->delay = 1;
}

// Places a bomb at the given position.
static void bomb_plant(Coords pos, u8 delay)
{
	Monster bomb = {.class = BOMB, .pos = pos, .next = player.next, .aggro = true, .delay = delay};
	player.next = &monsters[monster_count];
	monsters[monster_count++] = bomb;
}

static void bomb_tile(Tile *tile)
{
	tile->traps_destroyed = true;
	if (tile->monster)
		damage(tile->monster, 4, true);
	if ((tile->class == WALL && tile->hp < 5) || tile->class == WATER)
		tile->class = FLOOR;
	else if (tile->class == ICE)
		tile->class = WATER;
}

static void bomb_tick(Monster *this, __attribute__((unused)) Coords d)
{
	if (TILE(this->pos).monster == this)
		TILE(this->pos).monster = NULL;
	for (i64 x = this->pos.x - 1; x <= this->pos.x + 1; ++x)
		for (i64 y = this->pos.y - 1; y <= this->pos.y + 1; ++y)
			bomb_tile(&board[x][y]);
	monster_remove(this);
	bomb_exploded = true;
}

static void tile_change(Tile *tile, TileClass new_class)
{
	tile->class =
		tile->class == STAIRS ? STAIRS :
		tile->class * new_class == FIRE * ICE ? WATER :
		tile->class == WATER && new_class == FIRE ? FLOOR :
		new_class;
	tile->traps_destroyed = true;
}

// Kills the given monster, handling any on-death effects.
static void monster_kill(Monster *m, bool bomblike)
{
	if (m == &player)
		exit(DEATH);
	if (m->class == PIXIE || m->class == BOMBSHROOM_) {
		bomb_tick(m, spawn);
		return;
	}
	TILE(m->pos).monster = NULL;
	monster_remove(m);
	if (!bomblike && (m->class == WARLOCK_1 || m->class == WARLOCK_2))
		move(&player, m->pos);
	else if (m->class == ICE_SLIME || m->class == YETI)
		tile_change(&TILE(m->pos), ICE);
	else if (m->class == FIRE_SLIME || m->class == HELLHOUND)
		tile_change(&TILE(m->pos), FIRE);
	else if (m->class == BOMBER)
		bomb_plant(m->pos, 3);
	else if (m->class >= DIREBAT_1 && m->class <= OGRE)
		miniboss_defeated = true;
	else if (m->class == HARPY)
		harpies_defeated++;
}

// Deals damage to the given monster.
static void damage(Monster *m, i64 dmg, bool bomblike)
{
	if (m->class == MINE_STATUE) {
		bomb_tick(m, spawn);
	} else if (m->class == BOMBSHROOM) {
		m->class = BOMBSHROOM_;
		m->delay = 3;
	} else if (m->class == WIND_STATUE) {
		knockback(m);
	} else if (m->class == BOMB_STATUE) {
		knockback(m);
		m->delay = 2;
	} else if (dmg < 3 && (m->class == CRATE_1 || m->class == CRATE_2)) {
		knockback(m);
	} else if (IS_MIMIC(m->class) && m->state < 2) {
		return;
	} else if ((m->class == MOLE || m->class == GHOST) && m->state == 0) {
		return;
	} else if (dmg == 0) {
		return;
	} else if (!bomblike && (m->class == BLADENOVICE || m->class == BLADEMASTER) && m->state < 2) {
		knockback(m);
		m->state = 1;
	} else if (m->class >= RIDER_1 && m->class <= RIDER_3) {
		knockback(m);
		m->class += SKELETANK_1 - RIDER_1;
	} else if ((m->class == ARMADILLO_1 || m->class == ARMADILLO_2 || m->class == ARMADILDO) && m->state == 3) {
		m->prev_pos = player.pos;
	} else if (m->class == ICE_BEETLE || m->class == FIRE_BEETLE) {
		TileClass hazard = m->class == FIRE_BEETLE ? FIRE : ICE;
		Tile *tile = &TILE(m->pos);
		tile_change(tile, hazard);
		for (u64 i = 0; i < LENGTH(plus_shape); ++i)
			tile_change(tile + plus_shape[i], hazard);
		knockback(m);
	} else {
		m->hp -= dmg;
	}

	if (m->class == OOZE_GOLEM)
		tile_change(&TILE(player.pos), OOZE);

	if (m->hp <= 0) {
		monster_kill(m, bomblike);
	} else if (m->hp == 1 && m->class >= SKELETON_1 && m->class <= SKELETON_3) {
		m->class = HEADLESS;
		m->delay = 0;
		m->prev_pos = player.pos;
	} else if (IS_KNOCKED_BACK(m->class)) {
		knockback(m);
	}
}

static void lunge(Coords offset) {
	// lunging
	i64 steps = 4;
	while (--steps && can_move(&player, offset))
		move(&player, player.pos + offset);
	Monster *m = TILE(player.pos + offset).monster;
	if (steps && m) {
		knockback(m);
		damage(m, 4, true);
	}
}

// Attempts to move the player by the given offset.
// Will trigger attacking/digging if the destination contains an enemy/a wall.
static void player_move(i8 x, i8 y)
{
	if (sliding_on_ice)
		return;
	player.prev_pos = player.pos;
	if (!before_move(&player))
		return;
	Coords offset = {x, y};
	if (player.confusion)
		offset = -offset;
	Tile *dest = &TILE(player.pos + offset);
	if (dest->class == WALL) {
		dig(dest, TILE(player.pos).class == OOZE ? 0 : 2, false);
	} else if (IS_ENEMY(dest->monster)) {
		damage(dest->monster, TILE(player.pos).class == OOZE ? 0 : 5, false);
	} else {
		player_moved = true;
		move(&player, player.pos + offset);
		if (boots_on)
			lunge(offset);
		i64 digging_power = TILE(player.pos).class == OOZE ? 0 : 2;
		for (i64 i = 0; i < LENGTH(plus_shape); ++i)
			dig(&TILE(player.pos) + plus_shape[i], digging_power, false);
	}
}

// Deals bomb-like damage to all monsters on a horizontal line).
static void fireball(Coords pos, i8 dir)
{
	assert(dir != 0);
	for (pos.x += dir; TILE(pos).class != WALL; pos.x += dir)
		if (TILE(pos).monster)
			damage(TILE(pos).monster, 5, true);
}

// Freezes all monsters in a 3x5 cone.
static void cone_of_cold(Coords pos, i8 dir)
{
	for (i64 i = 0; i < LENGTH(cone_shape); ++i) {
		Tile *tile = &TILE(pos) + dir * cone_shape[i];
		if (tile->monster)
			tile->monster->freeze = 5;
	}
}
