// utils.c - core game logic

#define DIRECTION(pos) ((Coords) {SIGN((pos).x), SIGN((pos).y)})
#define L1(pos) (ABS((pos).x) + ABS((pos).y))
#define L2(pos) ((pos).x * (pos).x + (pos).y * (pos).y)

#define TILE(pos) (board[(pos).x][(pos).y])
#define CLASS(m) (class_infos[(m)->class])
#define NO_DIR ((Coords) {0, 0})

#define IS_OPAQUE(x, y) (board[x][y].class == WALL)
#define BLOCKS_MOVEMENT(pos) (TILE(pos).class == WALL)

static const Coords plus_shape[] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}, {0, 0}};
static const Coords cone_shape[] = {
	{1, 0},
	{2, -1}, {2, 1}, {2, 2},
	{3, -2}, {3, -1}, {3, 0}, {3, 1}, {3, 2},
};
static const Coords square_shape[] = {
	{-1, -1}, {-1, 0}, {-1, 1},
	{0, -1}, {0, 0}, {0, 1},
	{1, -1}, {1, 0}, {1, 1},
};

static Monster* monster_new(MonsterClass type, Coords pos) {
	Monster *new = &monsters[monster_count++];
	new->class = type;
	new->pos = new->prev_pos = pos;
	new->hp = CLASS(new).max_hp;
	return new;
}

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

static void adjust_lights(Coords pos, i8 diff) {
	static const i16 lights[33] = {
		102, 102, 102, -1, 102, 102, -1, -1, 102,
		94, 83, -1, -1, 53, -1, -1, 19, 10, 2,
	};
	Coords d = {0, 0};
	for (d.x = MAX(-4, -pos.x); d.x <= MIN(4, LENGTH(*board) - 1 - pos.x); ++d.x)
		for (d.y = MAX(-4, -pos.y); d.y <= MIN(4, LENGTH(*board) - 1 - pos.y); ++d.y)
			TILE(pos + d).light += diff * lights[L2(d)];
}

static void destroy_wall(Coords pos) {
	Tile *wall = &TILE(pos);
	assert(wall->class == WALL && wall->hp < 5);

	wall->class =
		wall->hp == 2 && wall->zone == 2 ? FIRE :
		wall->hp == 2 && wall->zone == 3 ? ICE :
		FLOOR;
	if (wall->monster && wall->monster->class == SPIDER) {
		wall->monster->class = FREE_SPIDER;
		wall->monster->delay = 1;
	}
	if (wall->torch)
		adjust_lights(pos, -1);
}

// Tries to dig away the given wall, replacing it with floor.
// Returns whether the dig succeeded.
static bool dig(Coords pos, i64 digging_power, bool z4)
{
	Tile *wall = &TILE(pos);

	// Doors are immune to Z4 AoE digging
	if (z4 && !wall->hp)
		return false;

	if (wall->class != WALL || wall->hp > digging_power)
		return false; // Dink!

	destroy_wall(pos);
	if (!z4 && wall->zone == 4 && (wall->hp == 1 || wall->hp == 2))
		for (i64 i = 0; i < 4; ++i)
			dig(pos + plus_shape[i], MIN(2, digging_power), true);
	return true;
}

static void damage_tile(Coords pos, Coords origin, i64 dmg, DamageType type) {
	if (TILE(pos).class == WALL && TILE(pos).hp < 5)
		destroy_wall(pos);
	if (TILE(pos).monster) {
		Coords dir = DIRECTION(pos - origin);
		if (dir.x && dir.y)
			dir.y = 0;
		damage(TILE(pos).monster, dmg, dir, type);
	}
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
// Usually boils down to damaging the player, but some enemies are special-cased.
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
			damage(&player, 1, d, DMG_NORMAL);
		break;
	default:
		damage(&player, 1, d, DMG_NORMAL);
	}
}

// Test whether a monster’s movement is blocked by freezing, water or tar.
// As a side effect, removes the water or frees the monster from tar, as appropriate.
// Common to all movement types (player, enemy, forced).
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
static MoveResult enemy_move(Monster *m, Coords offset)
{
	m->prev_pos = m->pos;
	m->delay = CLASS(m).beat_delay;
	if (!before_move(m))
		return MOVE_SPECIAL;
	if (m->confusion)
		offset = -offset;

	if (TILE(m->pos + offset).monster == &player) {
		enemy_attack(m);
		return MOVE_ATTACK;
	}
	if (can_move(m, offset)) {
		move(m, m->pos + offset);
		return MOVE_SUCCESS;
	}

	// Trampling
	if (!m->aggro && CLASS(m).dig == 4) {
		for (i64 i = 0; i < 4; ++i)
			damage_tile(m->pos + plus_shape[i], m->pos, 4, DMG_NORMAL);
		return MOVE_SPECIAL;
	}

	if (dig(m->pos + offset, m->confusion ? -1 : CLASS(m).dig, false)) {
		return MOVE_SPECIAL;
	}
	m->delay = 0;
	return MOVE_FAIL;
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

	if (TILE(dest).light < 102)
		return false;

	return los(dest.x - .55, dest.y - .55)
	    || los(dest.x + .55, dest.y - .55)
	    || los(dest.x - .55, dest.y + .55)
	    || los(dest.x + .55, dest.y + .55)
	    || los(dest.x, dest.y);
}

// Knocks an enemy away from the player.
static void knockback(Monster *m, Coords dir, u8 delay)
{
	forced_move(m, dir);
	m->delay = delay;
}

// Places a bomb at the given position.
static void bomb_plant(Coords pos, u8 delay)
{
	Monster bomb = {.class = BOMB, .pos = pos, .next = player.next, .aggro = true, .delay = delay};
	player.next = &monsters[monster_count];
	monsters[monster_count++] = bomb;
}

static void bomb_detonate(Monster *this, __attribute__((unused)) Coords d)
{
	if (TILE(this->pos).monster == this)
		TILE(this->pos).monster = NULL;
	for (i64 i = 0; i < 9; ++i) {
		Coords bombed = this->pos + square_shape[i];
		Tile *tile = &TILE(bombed);
		damage_tile(bombed, this->pos, 4, DMG_BOMB);
		tile->traps_destroyed = true;
		tile->class = tile->class == WATER ? FLOOR : tile->class == ICE ? WATER : tile->class;
	}
	monster_remove(this);
	bomb_exploded = true;
}

// Overrides a tile with a given floor hazard. Also destroys traps on the tile.
// Special cases: stairs are immune, fire+ice => water, fire+water => nothing.
static void tile_change(Tile *tile, TileClass new_class)
{
	tile->class =
		tile->class == STAIRS ? STAIRS :
		tile->class * new_class == FIRE * ICE ? WATER :
		tile->class == WATER && new_class == FIRE ? FLOOR :
		new_class;
	tile->traps_destroyed = true;
}

// Kills the given monster, handling on-death effects.
static void monster_kill(Monster *m, DamageType type)
{
	if (m == &player)
		exit(DEATH);
	if (m->class == PIXIE || m->class == BOMBSHROOM_) {
		bomb_detonate(m, spawn);
		return;
	}
	TILE(m->pos).monster = NULL;
	monster_remove(m);
	if (type == DMG_WEAPON && (m->class == WARLOCK_1 || m->class == WARLOCK_2))
		move(&player, m->pos);
	else if (m->class == ICE_SLIME || m->class == YETI)
		tile_change(&TILE(m->pos), ICE);
	else if (m->class == FIRE_SLIME || m->class == HELLHOUND)
		tile_change(&TILE(m->pos), FIRE);
	else if (m->class == BOMBER)
		bomb_plant(m->pos, 3);
	else if (m->class >= DIREBAT_1 && m->class <= OGRE)
		miniboss_defeated = true;
	else if (m->class >= SARCO_1 && m->class <= SARCO_3)
		sarcophagus_defeated = true;
	else if (m->class == HARPY)
		harpies_defeated++;
}

// Deals damage to the given monster. Handles on-damage effects.
static bool damage(Monster *m, i64 dmg, Coords dir, DamageType type)
{
	// Before-damage triggers that work even with 0 damage
	switch (m->class) {
	case MINE_STATUE:
		bomb_detonate(m, spawn);
		return false;
	case WIND_STATUE:
		if (type == DMG_BOMB)
			break;
		knockback(m, dir, 0);
		return false;
	case BOMB_STATUE:
		if (type == DMG_BOMB)
			break;
		knockback(m, dir, m->state ? 2 : 0);
		return false;
	case CRATE_1:
	case CRATE_2:
		if (dmg >= 3)
			break;
		knockback(m, dir, 1);
		return false;
	default:
		if (dmg == 0)
			return false;
	}

	// Before-damage triggers
	switch (m->class) {
	case BOMBSHROOM:
		m->class = BOMBSHROOM_;
		m->delay = 3;
		return false;
	case TARMONSTER:
	case WALL_MIMIC:
	case SEEK_STATUE:
	case FIRE_MIMIC:
	case ICE_MIMIC:
		if (type == DMG_BOMB || m->state == 2)
			break;
		return false;
	case MOLE:
	case GHOST:
		if (m->state == 1)
			break;
		return false;
	case BLADENOVICE:
	case BLADEMASTER:
		if (type != DMG_WEAPON || m->state == 2)
			break;
		knockback(m, dir, 1);
		m->state = 1;
		return false;
	case RIDER_1:
	case RIDER_2:
	case RIDER_3:
		knockback(m, dir, 1);
		m->class += SKELETANK_1 - RIDER_1;
		return false;
	case SKELETANK_1:
	case SKELETANK_2:
	case SKELETANK_3:
		if (m->vertical ? !dir.y : !dir.x)
			break;
		if (dmg >= m->hp)
			m->class = m->class - SKELETANK_1 + SKELETON_1;
		knockback(m, dir, 1);
		return false;
	case ARMADILLO_1:
	case ARMADILLO_2:
	case ARMADILDO:
		if (m->state != 3)
			break;
		m->prev_pos = player.pos;
		return false;
	case ICE_BEETLE:
	case FIRE_BEETLE:
		for (i64 i = 0; i < 5; ++i) {
			Tile *tile = &TILE(m->pos + plus_shape[i]);
			tile_change(tile, m->class == FIRE_BEETLE ? FIRE : ICE);
		}
		knockback(m, dir, 1);
		return false;
	case GOOLEM:
		tile_change(&TILE(player.pos), OOZE);
		break;
	default:
		break;
	}

	// Finally, deal the damage!
	m->hp -= dmg;
	if (m->hp <= 0) {
		monster_kill(m, type);
		return false;
	}

	// After-damage triggers
	switch (m->class) {
	case SKELETON_1:
	case SKELETON_2:
	case SKELETON_3:
	case SKELETANK_1:
	case SKELETANK_2:
	case SKELETANK_3:
		if (m->hp == 1) {
			m->class = HEADLESS;
			m->delay = 0;
			m->prev_pos = player.pos;
			return false;
		}
		return true;
	case MONKEY_2:
	case TELE_MONKEY:
	case ASSASSIN_2:
	case BANSHEE_1:
	case BANSHEE_2:
		knockback(m, dir, 1);
		return false;
	default:
		return true;
	}
}

static void lunge(Coords dir) {
	i64 steps = 4;
	while (--steps && can_move(&player, dir))
		move(&player, player.pos + dir);
	Tile *next = &TILE(player.pos + dir);
	if (steps && next->monster && damage(next->monster, 4, dir, DMG_NORMAL))
		knockback(next->monster, dir, 1);
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
		dig(player.pos + offset, TILE(player.pos).class == OOZE ? 0 : 2, false);
	} else if (dest->monster) {
		damage(dest->monster, TILE(player.pos).class == OOZE ? 0 : 5, offset, DMG_WEAPON);
	} else {
		player_moved = true;
		move(&player, player.pos + offset);
		if (boots_on)
			lunge(offset);

		// Miner’s cap
		i64 digging_power = TILE(player.pos).class == OOZE ? 0 : 2;
		for (i64 i = 0; i < 4; ++i)
			dig(player.pos + plus_shape[i], digging_power, false);
	}
}

// Deals bomb-like damage to all monsters on a horizontal line).
static void fireball(Coords pos, i8 dir)
{
	assert(dir != 0);
	for (pos.x += dir; TILE(pos).class != WALL; pos.x += dir)
		if (TILE(pos).monster)
			damage(TILE(pos).monster, 5, (Coords) {dir, 0}, DMG_NORMAL);
}

// Freezes all monsters in a 3x5 cone.
static void cone_of_cold(Coords pos, i8 dir)
{
	for (i64 i = 0; i < LENGTH(cone_shape); ++i) {
		Tile *tile = &TILE(pos + dir * cone_shape[i]);
		if (tile->monster)
			tile->monster->freeze = 5;
	}
}
