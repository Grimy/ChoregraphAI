// main.c - core game logic

#include "chore.h"

const Coords plus_shape[] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}, {0, 0}};
Coords spawn;
Coords stairs;

__extension__ __thread GameState g = {
	.board = {[0 ... 31] = {[0 ... 31] = {.class = EDGE}}},
	.inventory = { [BOMBS] = 3 },
	.boots_on = true,
};

// Some pre-declarations
static bool damage(Monster *m, i64 dmg, Coords dir, DamageType type);

Monster *monster_spawn(MonsterClass type, Coords pos)
{
	Monster *new = &g.monsters[++g.last_monster];
	assert(g.last_monster < ARRAY_SIZE(g.monsters));
	new->class = type;
	new->hp = CLASS(new).max_hp;
	new->pos = new->prev_pos = pos;
	return new;
}

// Moves the given monster to a specific position.
static void move(Monster *m, Coords dest)
{
	TILE(m->pos).monster = 0;
	m->untrapped = false;
	m->pos = dest;
	TILE(m->pos).monster = (u8) (m - g.monsters);
}

// Tests whether the given monster can move in the given direction.
bool can_move(Monster *m, Coords dir)
{
	assert(m != &player || L1(dir));
	Coords dest = m->pos + dir;
	if (TILE(dest).monster)
		return &MONSTER(m->pos + dir) == &player;
	if (m->class == SPIDER)
		return IS_DIGGABLE(dest) && !IS_DOOR(dest) && !TILE(dest).torch;
	if (m->class == MOLE && (TILE(dest).class == WATER || TILE(dest).class == TAR))
		return false;
	return !BLOCKS_LOS(dest);
}

void adjust_lights(Coords pos, i8 diff, i8 brightness)
{
	static const i16 lights[33] = {
		102, 102, 102, 102, 102, 102, 102, 102, 102,
		94, 83, -1, -1, 53, 43, 33, 19, 10, 2,
	};
	Coords d = {0, 0};
	assert(ARRAY_SIZE(g.board) == 32);
	for (d.x = -min(pos.x, 4); d.x <= min(4, 31 - pos.x); ++d.x)
		for (d.y = -min(pos.y, 4); d.y <= min(4, 31 - pos.y); ++d.y)
			TILE(pos + d).light += diff * lights[max(0, L2(d) - brightness)];
}

static void destroy_wall(Coords pos)
{
	assert(IS_DIGGABLE(pos));
	Tile *wall = &TILE(pos);

	wall->class -= 128;

	if (MONSTER(pos).class == SPIDER) {
		MONSTER(pos).class = FREE_SPIDER;
		MONSTER(pos).delay = 1;
	}

	for (i64 i = 0; wall->hp == 0 && i < 4; ++i)
		if (IS_DOOR(pos + plus_shape[i]))
			destroy_wall(pos + plus_shape[i]);

	if (wall->torch)
		adjust_lights(pos, -1, 0);
}

static void z4dig(Coords pos, i32 digging_power)
{
	if (TILE(pos).class == Z4WALL && TILE(pos).hp <= digging_power)
		destroy_wall(pos);
}

// Tries to dig away the given wall, replacing it with floor.
// Returns whether the dig succeeded.
static bool dig(Coords pos, i32 digging_power)
{
	if (!IS_DIGGABLE(pos) || TILE(pos).hp > digging_power)
		return false; // Dink!

	if (TILE(pos).class == Z4WALL)
		for (i64 i = 0; i < 4; ++i)
			z4dig(pos + plus_shape[i], digging_power);

	destroy_wall(pos);
	return true;
}

void damage_tile(Coords pos, Coords origin, i64 dmg, DamageType type)
{
	if (IS_DIGGABLE(pos))
		destroy_wall(pos);
	damage(&MONSTER(pos), dmg, CARDINAL(pos - origin), type);
}

void bomb_detonate(Monster *this, __attribute__((unused)) Coords d)
{
	static const Coords square_shape[] = {
		{-1, -1}, {-1, 0}, {-1, 1},
		{0, -1}, {0, 0}, {0, 1},
		{1, -1}, {1, 0}, {1, 1},
	};

	if (&MONSTER(this->pos) == this)
		TILE(this->pos).monster = 0;

	for (i64 i = 0; i < 9; ++i) {
		Tile *tile = &TILE(this->pos + square_shape[i]);
		if (this->class != PIXIE)
			tile->class = tile->class == WATER ? FLOOR : tile->class == ICE ? WATER : tile->class;
		if (i != 4 || this->class != PIXIE)
			tile->destroyed = true;
		damage_tile(this->pos + square_shape[i], this->pos, 4, DMG_BOMB);
	}

	this->hp = 0;
}

// Handles an enemy attacking the player.
// Usually boils down to damaging the player, but some enemies are special-cased.
void enemy_attack(Monster *attacker)
{
	Coords d = player.pos - attacker->pos;

	switch (attacker->class) {
	case MONKEY_1:
	case MONKEY_2:
	case CONF_MONKEY:
	case TELE_MONKEY:
		if (g.monkey)
			break;
		g.monkey = (u8) (attacker - g.monsters);
		TILE(attacker->pos).monster = 0;
		attacker->hp *= attacker->class == MONKEY_2 ? 3 : 4;
		break;
	case PIXIE:
		TILE(attacker->pos).monster = 0;
		attacker->hp = 0;
		player.hp += 2;
		break;
	case SHOVE_1:
	case SHOVE_2:
		if (forced_move(&player, d))
			move(attacker, attacker->pos + d);
		else
			damage(&player, 1, d, DMG_NORMAL);
		break;
	case BOMBER:
		bomb_detonate(attacker, NO_DIR);
		break;
	case WATER_BALL:
		tile_change(player.pos, WATER);
		TILE(attacker->pos).monster = 0;
		attacker->hp = 0;
		break;
	case GORGON_1:
	case GORGON_2:
		player.freeze = g.current_beat + 4;
		attacker->class = CRATE_1;
		break;
	default:
		damage(&player, CLASS(attacker).damage, d, DMG_NORMAL);
	}
}

// Tests whether a monster’s movement is blocked by water or tar.
// As a side effect, removes the water or frees the monster from tar, as appropriate.
// Common to all movement types (player, enemy, forced).
static bool is_bogged(Monster *m)
{
	if (TILE(m->pos).class == WATER && !CLASS(m).flying)
		TILE(m->pos).class = FLOOR;
	else if (TILE(m->pos).class == TAR && !CLASS(m).flying && !m->untrapped)
		m->untrapped = true;
	else
		return false;
	return true;
}

// Attempts to move the given monster in the given direction.
// Updates the enemy’s delay as appropriate.
// Can trigger attacking/digging if the destination contains the player/a wall.
// The return code indicates what action actually took place.
MoveResult enemy_move(Monster *m, Coords dir)
{
	m->prev_pos = m->pos;
	m->delay = CLASS(m).beat_delay;
	m->requeued = false;

	if (m->hp <= 0)
		return MOVE_FAIL;

	if (is_bogged(m))
		return MOVE_SPECIAL;
	if (m->confused && m->class != BARREL)
		dir = -dir;

	// Attack
	if (&MONSTER(m->pos + dir) == &player) {
		enemy_attack(m);
		return MOVE_ATTACK;
	}

	// Actual movement
	if (can_move(m, dir)) {
		move(m, m->pos + dir);
		return MOVE_SUCCESS;
	}

	// Try the move again after other monsters have moved
	Monster *blocker = &MONSTER(m->pos + dir);
	if (!m->was_requeued || (blocker->requeued && CLASS(blocker).priority < CLASS(m).priority)) {
		m->requeued = true;
		return MOVE_FAIL;
	}

	// Trampling
	i32 digging_power = m->confused ? -1 : CLASS(m).digging_power;
	if (!m->aggro && digging_power == 4) {
		for (i64 i = 0; i < 4; ++i)
			damage_tile(m->pos + plus_shape[i], m->pos, 4, DMG_NORMAL);
		return MOVE_SPECIAL;
	}

	// Digging
	digging_power += (m->class == MINOTAUR_1 || m->class == MINOTAUR_2) && m->state;
	if (dig(m->pos + dir, digging_power)) {
		return MOVE_SPECIAL;
	}

	m->delay = 0;
	return MOVE_FAIL;
}

void update_fov(void)
{
	Tile *tile = &TILE(player.pos);
	tile->revealed = true;
	cast_light(tile, +1, +ARRAY_SIZE(g.board));
	cast_light(tile, +1, -ARRAY_SIZE(g.board));
	cast_light(tile, -1, +ARRAY_SIZE(g.board));
	cast_light(tile, -1, -ARRAY_SIZE(g.board));
	cast_light(tile, +ARRAY_SIZE(g.board), +1);
	cast_light(tile, -ARRAY_SIZE(g.board), +1);
	cast_light(tile, +ARRAY_SIZE(g.board), -1);
	cast_light(tile, -ARRAY_SIZE(g.board), -1);
}

static void knockback(Monster *m, Coords dir, u8 delay)
{
	if (!m->knocked && !STUCK(m))
		forced_move(m, dir);
	m->knocked = true;
	m->delay = delay;
}

// Places a bomb at the given position.
static void bomb_plant(Coords pos, u8 delay)
{
	Monster *bomb = monster_spawn(BOMB, pos);
	bomb->delay = delay;
}

// Overrides a tile with a given floor hazard. Also destroys traps on the tile.
// Special cases: stairs are immune, fire+ice => water, fire+water => nothing.
void tile_change(Coords pos, TileClass new_class)
{
	Tile *tile = &TILE(pos);
	tile->class =
		tile->class == STAIRS ? STAIRS :
		BLOCKS_LOS(pos) ? tile->class :
		tile->class * new_class == FIRE * ICE ? WATER :
		tile->class == WATER && new_class == FIRE ? FLOOR :
		new_class;
	tile->destroyed = true;
}

// Kills the given monster, handling on-death effects.
void monster_kill(Monster *m, DamageType type)
{
	m->hp = 0;

	switch (m->class) {
	case LIGHTSHROOM:
		adjust_lights(m->pos, -1, 3);
		break;
	case ICE_SLIME:
	case YETI:
	case ICE_POT:
	case ICE_MIMIC:
		tile_change(m->pos, ICE);
		break;
	case FIRE_SLIME:
	case HELLHOUND:
	case FIRE_POT:
	case FIRE_MIMIC:
		tile_change(m->pos, FIRE);
		break;
	case WARLOCK_1:
	case WARLOCK_2:
		if (type == DMG_WEAPON)
			move(&player, m->pos);
		break;
	case BOMBER:
		bomb_plant(m->pos, 2);
		break;
	case WATER_BALL:
		tile_change(m->pos, WATER);
		break;
	case GORGON_1:
	case GORGON_2:
		m->class = CRATE_1;
		return;
	case SKULL_1:
	case SKULL_2:
	case SKULL_3:
		m->class -= SKULL_1 - SKELETON_1;
		m->delay = 1;
		m->hp = CLASS(m).max_hp;

		u8 spawned = 1;
		while (g.monsters[spawned].class)
			++spawned;

		for (u8 i = spawned; i <= spawned + 1; ++i) {
			g.monsters[i] = *m;
			g.monsters[i].pos += (i == spawned ? 1 : -1) * (Coords) {1, 0};
			TILE(g.monsters[i].pos).monster = i;
		}
		return;
	case SARCO_1 ... SARCO_3:
	case DIREBAT_1 ... EARTH_DRAGON:
		--g.locking_enemies;
		break;
	}

	if (MONSTER(m->pos).hp == 0)
		TILE(m->pos).monster = 0;

	if (m->item)
		TILE(m->pos).item = m->item;
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
			break; // Literally
		knockback(m, dir, 0);
		return false;
	case BARREL:
		if (dmg >= 3)
			break;
		// Do a barrel roll!
		m->prev_pos = m->pos - dir;
		return false;
	case TEH_URN:
		if (dmg >= 5)
			break;
		if (type != DMG_WEAPON) {
			dmg = 1;
			break;
		}
		knockback(m, dir, 0);
		return false;
	case CHEST:
		if (type == DMG_WEAPON)
			break;
		return false;
	}

	if (dmg == 0 || m->hp <= 0)
		return false;

	// Before-damage triggers
	switch (m->class) {
	case BOMBSHROOM:
		m->class = BOMBSHROOM_;
		m->delay = 3;
		return false;
	case TARMONSTER:
	case MIMIC_1:
	case MIMIC_2:
	case MIMIC_3:
	case MIMIC_4:
	case MIMIC_5:
	case WALL_MIMIC:
	case MIMIC_STATUE:
	case FIRE_MIMIC:
	case ICE_MIMIC:
	case SHOP_MIMIC:
		if (type == DMG_BOMB || m->state == 2)
			break;
		return false;
	case MOLE:
	case GHOST:
		if (m->state == 1)
			break;
		return true;
	case DEVIL_1:
	case DEVIL_2:
		if (m->state)
			break;
		m->state = 1;
		knockback(m, dir, 1);
		return false;
	case BLADENOVICE:
	case BLADEMASTER:
		if (type != DMG_WEAPON || m->state == 2)
			break;
		if (L1(m->pos - player.pos) == 1) {
			knockback(m, dir, 1);
			m->state = 1;
		}
		return true;
	case RIDER_1:
	case RIDER_2:
	case RIDER_3:
		m->class -= RIDER_1 - SKELETANK_1;
		knockback(m, dir, 1);
		return false;
	case SKELETANK_1:
	case SKELETANK_2:
	case SKELETANK_3:
		if (!coords_eq(dir, -m->dir))
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
		m->prev_pos = m->pos - dir;
		return false;
	case ICE_BEETLE:
	case FIRE_BEETLE:
		knockback(m, L1(m->pos - player.pos) == 1 ? NO_DIR : dir, 1);
		TileClass hazard = m->class == FIRE_BEETLE ? FIRE : ICE;
		for (i64 i = 0; i < 5; ++i)
			tile_change(m->pos + plus_shape[i], hazard);
		return false;
	case PIXIE:
	case BOMBSHROOM_:
		bomb_detonate(m, NO_DIR);
		return false;
	case GOOLEM:
		if (type == DMG_WEAPON && m->state == 0) {
			m->state = 1;
			tile_change(player.pos, OOZE);
		}
		break;
	case ORC_1:
	case ORC_2:
	case ORC_3:
		if (!coords_eq(dir, -m->dir))
			break;
		knockback(m, dir, 1);
		return false;
	case WIRE_ZOMBIE:
		if (IS_WIRE(player.pos) || !IS_WIRE(m->pos))
			break;
		knockback(m, dir, 2);
		return false;
	case PLAYER:
		if (g.iframes > g.current_beat)
			return false;
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
	case SKELETON_2:
	case SKELETON_3:
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
	case ASSASSIN_1:
	case ASSASSIN_2:
	case DEVIL_1:
	case DEVIL_2:
	case BANSHEE_1:
	case BANSHEE_2:
		knockback(m, dir, 1);
		return false;
	case METROGNOME_1:
	case METROGNOME_2:
		MONSTER(stairs).hp = 0;
		move(m, stairs);
		m->delay = 1;
		m->state = 1;
		return true;
	case PLAYER:
		g.iframes = g.current_beat + 2;
	}

	return true;
}

static void after_move(Coords dir, bool forced)
{
	if (g.inventory[LUNGING] && g.boots_on) {
		i64 steps = 4;
		while (--steps && !forced && can_move(&player, dir))
			move(&player, player.pos + dir);
		Monster *in_my_way = &MONSTER(player.pos + dir);
		if (steps && damage(in_my_way, 4, dir, DMG_NORMAL))
			knockback(in_my_way, dir, 1);
	}

	if (g.inventory[MEMERS_CAP]) {
		i32 digging_power = TILE(player.pos).class == OOZE ? 0 : 2;
		for (i64 i = 0; i < 4; ++i)
			dig(player.pos + plus_shape[i], digging_power);
	}
}

// Moves something by force (as caused by bounce traps, wind mages and knockback).
// Unlike enemy_move, ignores confusion, delay, and digging.
bool forced_move(Monster *m, Coords dir)
{
	assert(m != &player || L1(dir));
	if (m->freeze > g.current_beat || is_bogged(m) || (m == &player && g.monkey))
		return false;

	if (&MONSTER(m->pos + dir) == &player) {
		enemy_attack(m);
		return true;
	} else if (IS_EMPTY(m->pos + dir)) {
		m->prev_pos = m->pos;
		move(m, m->pos + dir);
		if (m == &player)
			after_move(dir, true);
		return true;
	}

	return false;
}

static void chain_lightning(Coords pos, Coords dir)
{
	Coords queue[32] = { pos + dir };
	i64 queue_length = 1;
	Coords arcs[7] = {
		{ dir.x, dir.y },
		{ dir.x + dir.y, dir.y - dir.x },
		{ dir.x - dir.y, dir.y + dir.x },
		{ dir.y, -dir.x },
		{ -dir.y, dir.x },
		{ dir.y - dir.x, -dir.y - dir.x },
		{ -dir.y - dir.x, dir.x - dir.y },
	};

	MONSTER(pos + dir).electrified = true;

	for (i64 i = 0; queue[i].x; ++i) {
		for (i64 j = 0; j < 7; ++j) {
			Monster *m = &MONSTER(queue[i] + arcs[j]);
			if (m->hp > 0 && !m->electrified) {
				m->electrified = true;
				damage(m, 1, CARDINAL(arcs[j]), DMG_NORMAL);
				queue[queue_length++] = queue[i] + arcs[j];
			}
		}
	}
}

// Attempts to move the player in the given direction
// Will trigger attacking/digging if the destination contains an enemy/a wall.
static void player_move(i8 x, i8 y)
{
	// While frozen or ice-sliding, the player can’t move on their own
	if (g.sliding_on_ice || player.freeze > g.current_beat)
		return;

	Tile *tile = &TILE(player.pos);
	player.prev_pos = player.pos;
	Coords dir = {x, y};
	i32 dmg = tile->class == OOZE ? 0 : g.inventory[JEWELED] ? 5 : 1;

	if (player.confused || (g.monkey && g.monsters[g.monkey].class == CONF_MONKEY))
		dir = -dir;

	if (g.monkey) {
		Monster *m = &g.monsters[g.monkey];
		m->hp -= max(1, dmg);
		if (m->hp <= 0)
			g.monkey = 0;
		if (m->class == TELE_MONKEY)
			monster_kill(&player, DMG_NORMAL);
		else if (m->class != CONF_MONKEY)
			return;
	}

	if (is_bogged(&player))
		return;

	Coords dest = player.pos + dir;

	if (BLOCKS_LOS(dest)) {
		dig(player.pos + dir, TILE(player.pos).class == OOZE ? 0 : 2);
	} else if (TILE(dest).monster) {
		damage(&MONSTER(dest), dmg, dir, DMG_WEAPON);
		if (IS_WIRE(player.pos))
			chain_lightning(player.pos, dir);
	} else {
		g.player_moved = true;
		move(&player, player.pos + dir);
		after_move(dir, false);
	}
}

// Deals normal damage to all monsters on a horizontal line.
void fireball(Coords pos, i8 dir)
{
	assert(dir != 0);
	for (pos.x += dir; !BLOCKS_LOS(pos); pos.x += dir)
		damage(&MONSTER(pos), 5, (Coords) {dir, 0}, DMG_NORMAL);
}

// Freezes all monsters in a 3x5 cone.
void cone_of_cold(Coords pos, i8 dir)
{
	static const Coords cone_shape[] = {
		{1, 0},
		{2, -1}, {2, 0}, {2, 1},
		{3, -2}, {3, -1}, {3, 0}, {3, 1}, {3, 2},
	};
	for (u64 i = 0; i < ARRAY_SIZE(cone_shape); ++i) {
		if (pos.x + dir * cone_shape[i].x >= 32)
			return;
		MONSTER(pos + dir * cone_shape[i]).freeze = g.current_beat + 5;
	}
}

// Tests whether the level has been cleared
bool player_won()
{
	return TILE(player.pos).class == STAIRS && g.locking_enemies == 0;
}

// Adds an item to the player’s inventory.
// Returns the item the player had in that slot.
ItemClass pickup_item(ItemClass item)
{
	if (item == BOMBS_3)
		g.inventory[BOMBS] += 3;
	else if (item == HEART_1)
		player.hp += 1;
	else if (item == HEART_2)
		player.hp += 2;
	else
		++g.inventory[item];
	return NO_ITEM;
}

static void player_turn(u8 input)
{
	g.player_moved = false;

	if (TILE(player.pos).item)
		TILE(player.pos).item = pickup_item(TILE(player.pos).item);

	switch (input) {
	case 'e':
		player_move(-1,  0);
		break;
	case 'f':
		player_move( 0,  1);
		break;
	case 'i':
		player_move( 1,  0);
		break;
	case 'j':
		player_move( 0, -1);
		break;
	case '<':
		if (g.inventory[BOMBS]) {
			--g.inventory[BOMBS];
			bomb_plant(player.pos, 3);
		}
		break;
	case ' ':
		g.boots_on ^= 1;
		break;
	}

	// Handle ice and fire
	if (g.sliding_on_ice)
		g.player_moved = forced_move(&player, DIRECTION(player.pos - player.prev_pos));
	else if (!g.player_moved && TILE(player.pos).class == FIRE)
		damage(&player, 2, NO_DIR, DMG_NORMAL);

	g.sliding_on_ice = g.player_moved && TILE(player.pos).class == ICE
		&& can_move(&player, DIRECTION(player.pos - player.prev_pos));
}

static bool check_aggro(Monster *m, Coords d, bool bomb_exploded)
{
	bool shadowed = g.nightmare && L2(m->pos - g.monsters[g.nightmare].pos) < 9;
	m->aggro = (d.y >= -5 && d.y <= 6)
		&& (d.x >= -10 && d.x <= 9)
		&& TILE(m->pos).revealed
		&& (TILE(m->pos).light >= 102
			|| L2(player.pos - m->pos) < 9
			|| shadowed);

	if (m->aggro && (m->class == BLUE_DRAGON || m->class == EARTH_DRAGON))
		return true;

	// The nightmare-bomb-aggro bug
	if (m->aggro && (bomb_exploded || shadowed) && CLASS(m).radius)
		return true;

	if (L2(d) <= CLASS(m).radius) {
		if (m->class >= SARCO_1 && m->class <= SARCO_3)
			m->delay = CLASS(m).beat_delay;
		return true;
	}

	return false;
}

static void trap_turn(Trap *this)
{
	if (TILE(this->pos).destroyed)
		return;

	Monster *m = &MONSTER(this->pos);
	if (m->untrapped || CLASS(m).flying)
		return;
	m->untrapped = true;

	switch (this->class) {
	case OMNIBOUNCE:
		if (L1(m->pos - m->prev_pos))
			forced_move(m, DIRECTION(m->pos - m->prev_pos));
		break;
	case BOUNCE:
		forced_move(m, this->dir);
		break;
	case SPIKE:
		damage(m, 4, NO_DIR, DMG_NORMAL);
		break;
	case TRAPDOOR:
	case TELEPORT:
		monster_kill(m, DMG_NORMAL);
		break;
	case CONFUSE:
		if (!(m->confused))
			m->confusion = g.current_beat + 10;
		break;
	case BOMBTRAP:
		if (m == &player)
			bomb_plant(this->pos, 2);
		break;
	case TEMPO_DOWN:
	case TEMPO_UP:
		break;
	}
}

// Compares the priorities of two monsters. Callback for qsort.
i32 compare_priorities(const void *a, const void *b)
{
	Monster *m1 = *(Monster * const *) a;
	Monster *m2 = *(Monster * const *) b;

	if (CLASS(m1).priority < CLASS(m2).priority)
		return 1;
	if (CLASS(m1).priority > CLASS(m2).priority)
		return -1;
	
	if (L2(m1->pos - player.pos) > L2(m2->pos - player.pos))
		return 1;
	if (L2(m1->pos - player.pos) < L2(m2->pos - player.pos))
		return -1;

	return 0;
}

// Runs one full beat of the game.
// During each beat, the player acts first, enemies second and traps last.
// Enemies act in decreasing priority order. Traps have an arbitrary order.
void do_beat(u8 input)
{
	g.input[g.current_beat++ & 31] = input;
	bool bomb_exploded = false;

	player_turn(input);
	if (player_won())
		return;
	update_fov();

	Monster *queue[64] = { 0 };
	u64 queue_length = 0;

	for (Monster *m = &player + 1; m->class; ++m) {
		m->knocked = false;
		m->requeued = false;
		m->was_requeued = false;
		if (!CLASS(m).act || m->hp <= 0)
			continue;
		if (!m->aggro && !check_aggro(m, player.pos - m->pos, bomb_exploded))
			continue;
		if (m->freeze > g.current_beat)
			continue;
		if (m->delay) {
			--m->delay;
			continue;
		}

		if (m->class == BOMB || m->class == BOMB_STATUE)
			bomb_exploded = true;
		queue[queue_length++] = m;
	}

	qsort(queue, queue_length, sizeof(Monster *), compare_priorities);

	for (u64 i = 0; i < queue_length; ++i) {
		Monster *m = queue[i];
		Coords d = player.pos - m->pos;
		u8 old_state = m->state;
		Coords old_dir = m->dir;
		CLASS(m).act(m, d);
		if (m->requeued) {
			m->state = old_state;
			m->dir = old_dir;
			m->was_requeued = true;
			queue[queue_length++] = m;
		}
	}

	for (Trap *t = g.traps; t->pos.x; ++t)
		trap_turn(t);
}
