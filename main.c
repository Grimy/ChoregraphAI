// main.c - core game logic

#include <math.h>

#include "chore.h"

const Coords plus_shape[] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}, {0, 0}};

// Position of the exit stairs.
Coords stairs;

// ID of the player’s character.
u64 character;

__extension__ thread_local GameState g = {
	.board = {[0 ... 31] = {[0 ... 31] = {.type = EDGE}}},
	.bombs = 3,
	.boots_on = true,
};

// Adds a new monster to the list. Unlike monster_spawn, this doesn’t add the
// monster to the board, so other monsters can move through it (used for bombs).
static void monster_new(u8 type, Coords pos, u8 delay)
{
	assert(g.last_monster < ARRAY_SIZE(g.monsters));
	Monster *m = &g.monsters[++g.last_monster];
	m->type = type;

	memcpy((void*) m, (const void*) &type_infos[type], 8);
	m->untrapped = m->flying;
	m->pos = pos;
	m->prev_pos = pos;
	m->delay = delay;
	m->aggro = true;
}

// Places a new monster of the given type, at the given position.
Monster* monster_spawn(u8 type, Coords pos, u8 delay)
{
	monster_new(type, pos, delay);
	TILE(pos).monster = g.last_monster;
	return &g.monsters[g.last_monster];
}

static void monster_transform(Monster* m, u8 type)
{
	m->type = type;
	m->damage = type_infos[type].damage;
	m->max_delay = type_infos[type].max_delay;
}

// Moves the given monster to a specific position.
static void move(Monster *m, Coords dest)
{
	TILE(m->pos).monster = 0;
	m->untrapped = m->flying;
	m->pos = dest;
	TILE(m->pos).monster = (u8) (m - g.monsters);
}

// Tests whether the given monster can move in the given direction.
bool can_move(const Monster *m, Coords dir)
{
	assert(m != &player || L1(dir));
	Coords dest = m->pos + dir;
	if (m->type == TARMONSTER && m->state == 0)
		return TILE(dest).type == TAR;
	if (TILE(dest).monster)
		return &MONSTER(m->pos + dir) == &player;
	if (m->type == SPIDER)
		return IS_DIGGABLE(dest) && !IS_DOOR(dest) && !TILE(dest).torch;
	if (m->type == MOLE && (TILE(dest).type == WATER || TILE(dest).type == TAR))
		return false;
	return !BLOCKS_LOS(dest);
}

// Recomputes the lighting of nearby tiles when a light source is created or destroyed.
// diff: +1 to add a light source, -1 to remove it.
// radius: the maximum L2 distance at which the light source still provides light.
void adjust_lights(Coords pos, i8 diff, double radius)
{
	Coords d = {0, 0};
	assert(ARRAY_SIZE(g.board) == 32);
	for (d.x = -min(pos.x, 4); d.x <= min(4, 31 - pos.x); ++d.x)
		for (d.y = -min(pos.y, 4); d.y <= min(4, 31 - pos.y); ++d.y)
			TILE(pos + d).light += diff * max(0, (int) (6100 * (radius - sqrt(L2(d)))));
}

// Overrides a tile with a given floor hazard. Also destroys traps on the tile.
// Special cases: stairs are immune, fire+ice => water, fire+water => nothing.
static void tile_change(Coords pos, u8 new_type)
{
	Tile *tile = &TILE(pos);
	tile->type =
		tile->type == STAIRS ? STAIRS :
		BLOCKS_LOS(pos) ? tile->type :
		tile->type * new_type == FIRE * ICE ? WATER :
		tile->type == WATER && new_type == FIRE ? FLOOR :
		new_type;
	tile->destroyed = true;
}

// Tries to dig away the given wall, replacing it with floor.
// Returns whether the dig succeeded.
bool dig(Coords pos, i32 digging_power, bool can_splash)
{
	if (!IS_DIGGABLE(pos) || TILE(pos).hp > digging_power)
		return false;

	Tile *wall = &TILE(pos);

	if (can_splash && wall->type == Z4WALL)
		for (i64 i = 0; i < 4; ++i)
			if (TILE(pos + plus_shape[i]).type != DOOR)
				dig(pos + plus_shape[i], min(digging_power, 2), false);

	wall->type &= ICE;

	if (wall->hp == 0)
		for (i64 i = 0; i < 4; ++i)
			dig(pos + plus_shape[i], 0, false);

	if (wall->torch)
		adjust_lights(pos, -1, 4.25);
	if (MONSTER(pos).type == SPIDER) {
		monster_transform(&MONSTER(pos), FREE_SPIDER);
		MONSTER(pos).delay = 1;
	}

	return true;
}

// Handles an enemy attacking the player.
// Usually boils down to damaging the player, but some enemies are special-cased.
static void enemy_attack(Monster *attacker)
{
	Coords d = player.pos - attacker->pos;

	switch (attacker->type) {
	case MONKEY_1:
	case MONKEY_2:
	case CONF_MONKEY:
	case TELE_MONKEY:
	case TARMONSTER:
		if (g.monkeyed)
			break; // One monkey at a time, please
		g.monkeyed = (u8) (attacker - g.monsters);
		move(attacker, player.pos);
		attacker->hp *= attacker->type == MONKEY_2 ? 3 : 4;
		break;
	case PIXIE:
		TILE(attacker->pos).monster = 0;
		attacker->hp = 0;
		player.hp += 2;
		break;
	case SHOVE_1:
	case SHOVE_2:
		if (forced_move(&player, d)) {
			move(attacker, attacker->pos + d);
			g.player_moved = true;
		} else {
			damage(&player, 1, d, DMG_NORMAL);
		}
		break;
	case BOMBER:
		bomb_detonate(attacker, Coords {});
		break;
	case WATER_BALL:
		tile_change(player.pos, WATER);
		TILE(attacker->pos).monster = 0;
		attacker->hp = 0;
		break;
	case GORGON_1:
	case GORGON_2:
		player.freeze = 4;
		monster_kill(attacker, DMG_NORMAL);
		break;
	default:
		damage(&player, attacker->damage, d, DMG_NORMAL);
	}
}

// Tries to free a monster from water/tar, removing the water as appropriate.
// Return code indicates success/failure.
static bool unbog(Monster *m)
{
	if (!IS_BOGGED(m))
		return false;
	TILE(m->pos).type = TILE(m->pos).type == TAR ? TAR : FLOOR;
	m->untrapped = true;
	return true;
}

// Attempts to move the given monster in the given direction.
// Updates the enemy’s delay as appropriate.
// Can trigger attacking/digging if the destination contains the player/a wall.
// The return code indicates what action actually took place.
MoveResult enemy_move(Monster *m, Coords dir)
{
	m->requeued = false;
	m->prev_pos = m->pos;
	assert(m->hp > 0);

	if (unbog(m))
		return MOVE_SPECIAL;

	if (m->confusion)
		dir = -dir;

	// Attack
	if (m->pos + dir == player.pos) {
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
	if (!m->was_requeued || (blocker->requeued && blocker->priority < m->priority)) {
		m->requeued = true;
		return MOVE_FAIL;
	}

	// Trampling
	i32 digging_power = m->confusion ? -1 : m->digging_power;
	if (!m->aggro && digging_power == 4) {
		for (i64 i = 0; i < 4; ++i) {
			Coords pos = m->pos + plus_shape[i];
			dig(pos, 4, false);
			damage(&MONSTER(pos), 4, plus_shape[i], DMG_NORMAL);
		}
		return MOVE_SPECIAL;
	}

	// Digging
	digging_power += (m->type == MINOTAUR_1 || m->type == MINOTAUR_2) && m->state;
	if (dig(m->pos + dir, digging_power, m->type == DIGGER)) {
		return MOVE_SPECIAL;
	}

	m->delay = 0;
	return MOVE_FAIL;
}

// Marks tiles that the player can see as “revealed”. Uses a shadow-casting algorithm.
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

// Knockback is a special kind of forced movement that can be caused by damage.
// It can only affect a target once per beat, and changes its delay.
static void knockback(Monster *m, Coords dir, u8 delay)
{
	if (!m->knocked && !IS_BOGGED(m))
		forced_move(m, dir);
	m->knocked = true;
	m->delay = delay;
}

// Kills the given monster, handling on-death effects.
void monster_kill(Monster *m, DamageType type)
{
	m->hp = 0;
	if (m->item)
		TILE(m->pos).item = m->item;

	switch (m->type) {
	case TARMONSTER:
		tile_change(m->pos, TAR);
		[[clang::fallthrough]];
	case MONKEY_1:
	case MONKEY_2:
	case CONF_MONKEY:
	case TELE_MONKEY:
		if (m == &g.monsters[g.monkeyed]) {
			g.monkeyed = 0;
			TILE(player.pos).monster = 1;
			return;
		}
		break;
	case LIGHTSHROOM:
		adjust_lights(m->pos, -1, 4.5);
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
		monster_new(BOMB, m->pos, 3);
		break;
	case WATER_BALL:
		tile_change(m->pos, WATER);
		break;
	case GORGON_1:
		monster_spawn(STONE_STATUE, m->pos, 0);
		return;
	case GORGON_2:
		monster_spawn(GOLD_STATUE, m->pos, 0);
		return;
	case SARCO_1 ... SARCO_3:
	case DIREBAT_1 ... EARTH_DRAGON:
		--g.locking_enemies;
		if (m->type == NIGHTMARE_1 || m->type == NIGHTMARE_2)
			g.nightmare = 0;
		break;
	}

	TILE(m->pos).monster = 0;
}

// Spawns three skeletons in a line.
static void skull_spawn(const Monster *skull, Coords spawn_dir, Coords dir)
{
	u8 spawn_type = skull->type - SKULL_2 + SKELETON_2;
	for (i8 i = -1; i <= 1; ++i) {
		Coords pos = skull->pos + spawn_dir * i;
		if (IS_DIGGABLE(pos))
			dig(pos, 4, false);
		if (IS_EMPTY(pos)) {
			Monster *skele = monster_spawn(spawn_type, pos, 1);
			skele->dir = dir;
			skele->electrified = true;
		}
	}
}

// Deals damage to the given monster. Handles on-damage effects.
bool damage(Monster *m, i64 dmg, Coords dir, DamageType type)
{
	// Crates and gargoyles can be pushed even with 0 damage
	switch (m->type) {
	case MINE_STATUE:
		bomb_detonate(m, Coords {});
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
	case STONE_STATUE:
		knockback(m, dir, 0);
		return false;
	case GOLD_STATUE:
		dmg = 1;
		break;
	}

	if (!dmg || !m->hp)
		return false;

	// Before-damage triggers
	switch (m->type) {
	case BOMBSHROOM:
		monster_transform(m, BOMBSHROOM_);
		m->delay = 3;
		return false;
	case TARMONSTER:
	case MIMIC_1:
	case MIMIC_2:
	case MIMIC_3:
	case MIMIC_4:
	case MIMIC_5:
	case WHITE_MIMIC:
	case WALL_MIMIC:
	case MIMIC_STATUE:
	case FIRE_MIMIC:
	case ICE_MIMIC:
	case SHOP_MIMIC:
	case SHRINE:
		if (type == DMG_BOMB || m->state == 2)
			break;
		return true;
	case MOLE:
	case GHOST:
		if (m->state == 1)
			break;
		return true;
	case ORB_1:
	case ORB_2:
	case ORB_3:
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
		monster_transform(m, m->type - RIDER_1 + SKELETANK_1);
		m->flying = m->untrapped = false;
		knockback(m, dir, 1);
		return false;
	case SKELETANK_1:
	case SKELETANK_2:
	case SKELETANK_3:
		if (dir != -m->dir)
			break;
		if (dmg >= TYPE(m).max_hp)
			monster_transform(m, m->type - SKELETANK_1 + SKELETON_1);
		knockback(m, dir, 1);
		return false;
	case ARMADILLO_1:
	case ARMADILLO_2:
	case ARMADILDO:
		if (!m->state)
			break;
		m->prev_pos = m->pos - dir;
		return false;
	case ICE_BEETLE:
	case FIRE_BEETLE:
		knockback(m, L1(m->pos - player.pos) == 1 ? Coords {} : dir, 1);
		for (i64 i = 0; i < 5; ++i)
			tile_change(m->pos + plus_shape[i], m->type == FIRE_BEETLE ? FIRE : ICE);
		return false;
	case PIXIE:
	case BOMBSHROOM_:
		bomb_detonate(m, Coords {});
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
		if (dir != -m->dir)
			break;
		knockback(m, dir, 1);
		return false;
	case SKULL_1:
		m->type = SKULL_2 - 1;
		[[clang::fallthrough]];
	case SKULL_2:
	case SKULL_3:
		monster_kill(m, DMG_NORMAL);
		skull_spawn(m, { dir.x == 0, dir.x != 0 }, -CARDINAL(dir));
		return false;
	case WIRE_ZOMBIE:
		if (IS_WIRE(player.pos) || !(IS_WIRE(m->pos) || TILE(m->pos).type == STAIRS))
			break;
		knockback(m, dir, 2);
		return false;
	case PLAYER:
		if (g.iframes > g.current_beat)
			return false;
		m->freeze = 0;
		break;
	}

	// Handle death
	if (dmg >= m->hp) {
		monster_kill(m, type);
		return false;
	}

	// Finally, deal the damage!
	m->hp -= dmg;

	// After-damage triggers
	switch (m->type) {
	case SKELETON_2:
	case SKELETON_3:
	case SKELETANK_2:
	case SKELETANK_3:
		if (m->hp > 1)
			break;
		static_assert(SKELETON_2 & 1, "");
		static_assert(SKELETANK_2 & 1, "");
		monster_transform(m, m->type & 1 ? HEADLESS_2 : HEADLESS_3);
		m->delay = 0;
		m->prev_pos = m->pos - dir;
		return false;
	case MONKEY_2:
	case TELE_MONKEY:
	case ASSASSIN_1:
	case ASSASSIN_2:
	case GORGON_2:
	case GOLD_STATUE:
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
		break;
	}

	return true;
}

// Handles effects that trigger after the player’s movement.
static void after_move(Coords dir, bool forced)
{
	if (g.feet == FEET_LUNGING && g.boots_on) {
		i64 steps = 4;
		while (--steps && !forced && can_move(&player, dir))
			move(&player, player.pos + dir);
		Monster *in_my_way = &MONSTER(player.pos + dir);
		if (steps && damage(in_my_way, 4, dir, DMG_NORMAL))
			knockback(in_my_way, dir, 1);
	} else if (g.feet == FEET_LEAPING) {
		if (g.boots_on && can_move(&player, dir))
			move(&player, player.pos + dir);
		if (IS_BOGGED(&player))
			player.untrapped = true;
	}

	if (g.head == HEAD_MINERS) {
		i32 digging_power = TILE(player.pos).type == OOZE ? 0 : player.digging_power;
		for (i64 i = 0; i < 4; ++i)
			dig(player.pos + plus_shape[i], digging_power, true);
	}

	if (g.monkeyed)
		move(&g.monsters[g.monkeyed], player.pos);

	if (TILE(player.pos).item)
		TILE(player.pos).item = pickup_item(TILE(player.pos).item);
}

// Moves something by force (as caused by bounce traps, wind mages and knockback).
// Unlike enemy_move, ignores confusion, delay, and digging.
bool forced_move(Monster *m, Coords dir)
{
	assert(m != &player || L1(dir));
	if (m->freeze || unbog(m) || (m == &player && g.monkeyed))
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
	Coords queue[32] = { pos };
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

	MONSTER(pos).electrified = true;

	for (i64 i = 0; queue[i].x; ++i) {
		for (i64 j = 0; j < 7; ++j) {
			Monster *m = &MONSTER(queue[i] + arcs[j]);
			if (m->electrified)
				continue;
			m->electrified = true;
			damage(m, 1, CARDINAL(arcs[j]), DMG_NORMAL);
			queue[queue_length++] = queue[i] + arcs[j];
		}
	}
}

// Attempts to move the player in the given direction
// Will trigger attacking/digging if the destination contains an enemy/a wall.
static void player_move(i8 x, i8 y)
{
	// While frozen or ice-sliding, the player can’t move on their own
	if (g.sliding_on_ice || player.freeze)
		return;

	Tile *tile = &TILE(player.pos);
	player.prev_pos = player.pos;
	Coords dir = {x, y};
	i32 dmg = tile->type == OOZE ? 0 : player.damage;

	if (player.confusion || g.monsters[g.monkeyed].type == CONF_MONKEY)
		dir = -dir;

	if (g.monkeyed) {
		Monster *m = &g.monsters[g.monkeyed];
		damage(m, max(1, dmg), Coords {}, DMG_NORMAL);
		if (m->type == TELE_MONKEY)
			monster_kill(&player, DMG_NORMAL);
		else if (m->type != CONF_MONKEY)
			return;
	}

	if (unbog(&player))
		return;

	Coords dest = player.pos + dir;

	if (BLOCKS_LOS(dest)) {
		dig(player.pos + dir, TILE(player.pos).type == OOZE ? 0 : player.digging_power, true);
	} else if (TILE(dest).monster) {
		damage(&MONSTER(dest), dmg, dir, DMG_WEAPON);
		player.electrified = true;
		if (IS_WIRE(player.pos))
			chain_lightning(dest, dir);
	} else {
		g.player_moved = true;
		move(&player, player.pos + dir);
		after_move(dir, false);
	}
}

// Adds an item to the player’s inventory.
// Returns the item the player had in that slot.
u8 pickup_item(u8 item)
{
	switch (item) {
	case HEART_1:
		player.hp += 1;
		return NO_ITEM;
	case HEART_2:
		player.hp += 2;
		return NO_ITEM;
	case BOMB_1:
		g.bombs += 1;
		return NO_ITEM;
	case BOMB_3:
		g.bombs += 3;
		return NO_ITEM;
	case SHOVEL_BASE:
	case SHOVEL_TIT:
		SWAP(item, g.shovel);
		break;
	case DAGGER_BASE:
	case DAGGER_JEWELED:
		SWAP(item, g.weapon);
		player.damage = g.weapon == DAGGER_JEWELED ? 5 : 1;
		break;
	case HEAD_MINERS:
	case HEAD_CIRCLET:
		SWAP(item, g.head);
		player.radius = g.head == HEAD_MINERS ? 5 : 2;
		break;
	case FEET_LUNGING:
	case FEET_LEAPING:
		SWAP(item, g.feet);
		break;
	}

	player.digging_power = 1 + (g.shovel == SHOVEL_TIT) + (g.head == HEAD_MINERS);
	return item;
}

static void player_turn(u8 input)
{
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
	case 'z':
		if (!g.usable)
			break;
		g.usable = NO_ITEM;
		for (Monster *m = &player + 1; m->type; ++m)
			m->freeze = 16;
		break;
	case '<':
		if (g.bombs) {
			--g.bombs;
			monster_new(BOMB, player.pos, 3);
		}
		break;
	case ' ':
		g.boots_on ^= 1;
		break;
	}

	if (g.sliding_on_ice)
		g.player_moved = forced_move(&player, DIRECTION(player.pos - player.prev_pos));
}

static bool check_aggro(Monster *m, Coords d, bool bomb_exploded)
{
	bool shadowed = g.nightmare && L2(m->pos - g.monsters[g.nightmare].pos) < 8;
	m->aggro = (d.y >= -5 && d.y <= 6)
		&& (d.x >= -9 && d.x <= 10)
		&& TILE(m->pos).revealed
		&& (TILE(m->pos).light >= 7777
			|| shadowed
			|| (m->type >= DIREBAT_1 && m->type <= EARTH_DRAGON)
			|| L2(player.pos - m->pos) < 8);

	if (m->aggro && (m->type == BLUE_DRAGON || m->type == EARTH_DRAGON))
		return true;

	// The nightmare-bomb-aggro bug
	if (m->aggro && (bomb_exploded || shadowed) && m->radius)
		return true;

	if (L2(d) <= m->radius) {
		if (m->type >= SARCO_1 && m->type <= SARCO_3)
			m->delay = m->max_delay;
		return true;
	}

	return false;
}

static void trap_turn(const Trap *trap)
{
	if (TILE(trap->pos).destroyed)
		return;

	Monster *m = &MONSTER(trap->pos);

	if (m->untrapped)
		return;
	m->untrapped = true;

	switch (trap->type) {
	case OMNIBOUNCE:
		if (L1(m->pos - m->prev_pos))
			forced_move(m, DIRECTION(m->pos - m->prev_pos));
		break;
	case BOUNCE:
		forced_move(m, trap->dir);
		break;
	case SPIKE:
		damage(m, 4, Coords {}, DMG_NORMAL);
		break;
	case TRAPDOOR:
	case TELEPORT:
		monster_kill(m, DMG_NORMAL);
		break;
	case CONFUSE:
		if (!m->confusion && m->type != BARREL)
			m->confusion = 10;
		break;
	case BOMBTRAP:
		if (m == &player)
			monster_new(BOMB, player.pos, 2);
		break;
	case TEMPO_DOWN:
	case TEMPO_UP:
		break;
	}
}

// Tests whether the first monster has priority over the second.
static bool has_priority(const Monster *m1, const Monster *m2)
{
	if (m1->priority > m2->priority)
		return true;
	if (m1->priority < m2->priority)
		return false;

	// Optimization: if they’re further away than that, priority doesn’t matter
	if (L1(m1->pos - m2->pos) < 5)
		return L2(m1->pos - player.pos) < L2(m2->pos - player.pos);

	// We default to false to avoid needless swaps
	return false;
}

// Inserts a monster at its due place in the priority queue.
static void priority_insert(Monster **queue, u64 queue_length, Monster *m)
{
	u64 i = queue_length;
	while (i > 0 && has_priority(m, queue[i - 1]))
		--i;
	memmove(&queue[i + 1], &queue[i], (queue_length - i) * sizeof(Monster*));
	queue[i] = m;
}

static void before_and_after()
{
	for (i64 i = 0; i < 4; ++i) {
		Coords pos = player.pos + plus_shape[i];
		Monster *m = &MONSTER(pos);
		if (m->type == FIRE_BEETLE || m->type == ICE) {
			tile_change(m->pos, m->type == FIRE_BEETLE ? FIRE : ICE);
			m->type = BEETLE;
		}
	}
}

// Runs one full beat of the game.
// During each beat, the player acts first, enemies second and traps last.
// Enemies act in decreasing priority order. Traps have an arbitrary order.
bool do_beat(u8 input)
{
	// Player’s turn
	g.input[g.current_beat++ & 31] = input;

	player_turn(input);
	if (TILE(player.pos).type == STAIRS && g.locking_enemies == 0)
		return true;
	update_fov();
	before_and_after();

	// Build a priority queue with all active enemies
	Monster *queue[64] = { 0 };
	u64 queue_length = 0;
	bool bomb_exploded = false;

	for (Monster *m = &g.monsters[g.last_monster]; m > &player; --m) {
		if (!TYPE(m).act || !m->hp)
			continue;

		m->knocked = false;
		if (!m->aggro && !check_aggro(m, player.pos - m->pos, bomb_exploded))
			continue;

		if (m->type == BOMB || m->type == BOMB_STATUE)
			bomb_exploded = true;

		priority_insert(queue, queue_length++, m);
	}

	// Enemies’ turns
	for (u64 i = 0; i < queue_length; ++i) {
		Monster *m = queue[i];
		m->requeued = false;

		// We need to check again: an earlier enemy could have killed/frozen this one
		if (!m->hp || m->freeze)
			continue;

		if (m->delay) {
			--m->delay;
			continue;
		}

		u8 old_state = m->state;
		Coords old_dir = m->dir;
		m->delay = m->max_delay;
		TYPE(m).act(m, player.pos - m->pos);

		if (m->requeued) {
			// Undo all side-effects and add it to the back of the priority queue
			m->state = old_state;
			m->dir = old_dir;
			m->delay = 0;
			m->was_requeued = true;
			queue[queue_length++] = m;
		} else if (L2(player.pos - m->pos) < 8) {
			m->aggro = true;
		}
	}

	// Ice and Fire
	g.sliding_on_ice = g.player_moved && TILE(player.pos).type == ICE
		&& can_move(&player, DIRECTION(player.pos - player.prev_pos));

	if (!g.player_moved && TILE(player.pos).type == FIRE)
		damage(&player, 2, Coords {}, DMG_NORMAL);

	g.player_moved = false;

	// Traps’ turns
	for (Trap *t = g.traps; t->pos.x; ++t)
		trap_turn(t);

	// Flag upkeep
	for (Monster *m = &player; m->type; ++m) {
		if (!m->hp)
			continue;
		m->electrified = false;
		m->knocked = false;
		assert(!m->requeued);
		m->was_requeued = false;
		if (m->freeze)
			--m->freeze;
		if (m->confusion)
			--m->confusion;
		if (m->exhausted && m->aggro)
			--m->exhausted;
		if (m->type == EFREET)
			tile_change(m->pos, FIRE);
		else if (m->type == DJINN)
			tile_change(m->pos, ICE);
	}

	before_and_after();
	return false;
}
