// utils.c - core game logic

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
	for (d.x = -4; d.x <= 4; ++d.x)
		for (d.y = -4; d.y <= 4; ++d.y)
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
static bool dig(Coords pos, i32 digging_power, bool z4)
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
			dig(pos + plus_shape[i], min(2, digging_power), true);
	return true;
}

static void damage_tile(Coords pos, Coords origin, i64 dmg, DamageType type) {
	if (TILE(pos).class == WALL && TILE(pos).hp < 5)
		destroy_wall(pos);
	if (TILE(pos).monster)
		damage(TILE(pos).monster, dmg, CARDINAL(pos - origin), type);
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
		attacker->hp = 0;
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

// Tests whether the player can see the tile at the given position.
// This is true if there’s an unblocked line from the center of the player’s
// tile to any corner or the center of the destination tile.
static bool can_see(Coords dest)
{
	Coords pos = player.pos;
	if (dest.x < pos.x - 10 || dest.x > pos.x + 9 || dest.y < pos.y - 5 || dest.y > pos.y + 5)
		return false;

	// Miner’s Cap
	if (L2(dest - pos) < 6)
		return true;

	return TILE(dest).revealed;
}

static void cast_light(i8 row, double start, double end, Coords x, Coords y)
{
begin:
	if (start > end || row > 10)
		return;

	bool blocked = false;
	Coords delta = {row, 0};

	for (delta.y = 0; delta.y < row + 1; ++delta.y) {
		Coords current = {delta.x * x.x + delta.y * x.y, delta.x * y.x + delta.y * y.y};
		if (abs(current.y) > 5)
			continue;

		current += player.pos;
		double left_slope = (delta.y - 0.51) / (delta.x + 0.51);
		double right_slope = (delta.y + 0.51) / (delta.x - 0.51);

		if (current.x < 0 || (u8) current.x > ARRAY_SIZE(g.board) ||
		    current.y < 0 || (u8) current.y > ARRAY_SIZE(*g.board) || right_slope < start)
			continue;
		if (left_slope > end)
			break;

		TILE(current).revealed = TILE(current).light >= 102;

		bool was_blocked = blocked;
		blocked = TILE(current).class == WALL;
		if (!was_blocked && blocked)
			cast_light(row + 1, start, left_slope, x, y);
		if (blocked)
			start = right_slope;
	}
	++row;
	goto begin;
}

static void update_fov()
{
	static const Coords diagonals[] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
	TILE(player.pos).revealed = true;
	for (i64 i = 0; i < 4; ++i) {
		Coords d = diagonals[i];
		cast_light(1, 0, 1, (Coords) {0, d.x}, (Coords) {d.y, 0});
		cast_light(1, 0, 1, (Coords) {d.x, 0}, (Coords) {0, d.y});
	}
}

// Knocks an enemy away from the player.
static void knockback(Monster *m, Coords dir, u8 delay)
{
	if (dir.x || dir.y)
		forced_move(m, dir);
	m->delay = delay;
}

// Places a bomb at the given position.
static void bomb_plant(Coords pos, u8 delay)
{
	Monster *bomb;
	for (bomb = g.monsters; bomb->hp > 0; ++bomb);
	assert(bomb->class == BOMB);
	bomb->hp = 1;
	bomb->pos = pos;
	bomb->delay = delay;
}

static void bomb_detonate(Monster *this, __attribute__((unused)) Coords d)
{
	if (TILE(this->pos).monster == this)
		TILE(this->pos).monster = NULL;
	for (i64 i = 0; i < 9; ++i) {
		Tile *tile = &TILE(this->pos + square_shape[i]);
		tile->traps_destroyed = true;
		tile->class = tile->class == WATER ? FLOOR : tile->class == ICE ? WATER : tile->class;
	}
	for (i64 i = 0; i < 9; ++i)
		damage_tile(this->pos + square_shape[i], this->pos, 4, DMG_BOMB);
	this->hp = 0;
	g.bomb_exploded = true;
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
	m->hp = 0;

	if (m->class == PIXIE || m->class == BOMBSHROOM_) {
		bomb_detonate(m, NO_DIR);
		return;
	}

	TILE(m->pos).monster = NULL;

	if (type == DMG_WEAPON && (m->class == WARLOCK_1 || m->class == WARLOCK_2))
		move(&player, m->pos);
	else if (m->class == ICE_SLIME || m->class == YETI)
		tile_change(&TILE(m->pos), ICE);
	else if (m->class == FIRE_SLIME || m->class == HELLHOUND)
		tile_change(&TILE(m->pos), FIRE);
	else if (m->class == BOMBER)
		bomb_plant(m->pos, 3);
	else if (m->class >= DIREBAT_1 && m->class <= OGRE)
		g.miniboss_killed = true;
	else if (m->class >= SARCO_1 && m->class <= SARCO_3)
		g.sarcophagus_killed = true;
	else if (m->class == HARPY)
		g.harpies_killed++;
}

// Deals damage to the given monster. Handles on-damage effects.
static bool damage(Monster *m, i64 dmg, Coords dir, DamageType type)
{
	// Crates and gargoyles can be pushed even with 0 damage
	switch (m->class) {
	case MINE_STATUE:
		bomb_detonate(m, NO_DIR);
		return false;
	case WIND_STATUE:
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
	}

	if (dmg == 0)
		return false;

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
		knockback(m, dir, 1);
		for (i64 i = 0; i < 5; ++i) {
			Tile *tile = &TILE(m->pos + plus_shape[i]);
			tile_change(tile, m->class == FIRE_BEETLE ? FIRE : ICE);
		}
		return false;
	case GOOLEM:
		tile_change(&TILE(player.pos), OOZE);
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
		if (m->hp > 1)
			break;
		m->class = HEADLESS;
		m->delay = 0;
		m->prev_pos = player.pos;
		return false;
	case MONKEY_2:
	case TELE_MONKEY:
	case ASSASSIN_2:
	case BANSHEE_1:
	case BANSHEE_2:
		knockback(m, dir, 1);
		return false;
	}

	return true;
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
	if (g.sliding_on_ice)
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
		g.player_moved = true;
		move(&player, player.pos + offset);
		if (g.boots_on)
			lunge(offset);

		// Miner’s cap
		i32 digging_power = TILE(player.pos).class == OOZE ? 0 : 2;
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
	for (u64 i = 0; i < ARRAY_SIZE(cone_shape); ++i) {
		Tile *tile = &TILE(pos + dir * cone_shape[i]);
		if (tile->monster)
			tile->monster->freeze = 5;
	}
}

static bool player_won() {
	return TILE(player.pos).class == STAIRS
		&& g.miniboss_killed
		&& g.sarcophagus_killed;
}
