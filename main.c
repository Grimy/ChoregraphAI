// main.c - core game logic

#include <math.h>
#include <setjmp.h>

#include "chore.h"

static thread_local jmp_buf player_died;

thread_local GameState g = {
	.board = {[0 ... 31] = {[0 ... 31] = {.type = EDGE}}},
	.monsters = {{.untrapped = true, .electrified = true}},
	.weapon = DAGGER,
	.shovel = SHOVEL_BASIC,
	.bombs = 0,
	.boots_on = true,
};

// Adds a new monster to the list. Unlike monster_spawn, this doesn’t add the
// monster to the board, so other monsters can move through it (used for bombs).
static void monster_new(u8 type, Coords pos, u8 delay)
{
	assert(g.last_monster < ARRAY_SIZE(g.monsters));
	Monster *m = &g.monsters[++g.last_monster];
	*m = proto[type];
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
	m->damage = proto[type].damage;
	m->max_delay = proto[type].max_delay;
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
	assert(m != &player || dir != Coords {});
	Coords dest = m->pos + dir;
	if (m->type == TARMONSTER && m->state == 0)
		return TILE(dest).type == TAR;
	if (TILE(dest).monster)
		return &MONSTER(m->pos + dir) == &player;
	if (m->type == SPIDER)
		return IS_DIGGABLE(dest) && !IS_DOOR(dest) && !TILE(dest).torch;
	if (m->type == MOLE && (TILE(dest).type & WATER))
		return false;
	return !BLOCKS_LOS(dest);
}

// Recomputes the lighting of nearby tiles when a light source is created or destroyed.
// diff: +1 to add a light source, -1 to remove it.
// radius: the maximum L2 distance at which the light source still provides light.
void adjust_lights(Coords pos, i64 diff, double radius)
{
	assert(ARRAY_SIZE(g.board) == 32);
	Tile *tile = &TILE(pos);
	for (i64 x = -min(pos.x, 4); x <= min(4, 31 - pos.x); ++x) {
		for (i64 y = -min(pos.y, 4); y <= min(4, 31 - pos.y); ++y) {
			i64 light = (i64) (6100 * (radius - sqrt(x*x + y*y)));
			tile[32*x + y].light += diff * max(0, light);
		}
	}
}

// Overrides a tile with a given floor hazard. Also destroys traps on the tile.
// Special cases: stairs are immune, fire+ice => water, fire+water => nothing.
static void tile_change(Coords pos, u8 new_type)
{
	Tile *tile = &TILE(pos);
	tile->type =
		tile->type >= STAIRS ? tile->type :
		(tile->type ^ new_type) == (FIRE ^ ICE) ? WATER :
		tile->type == WATER && new_type == FIRE ? FLOOR :
		new_type;
	tile->destroyed = true;
}

// Tries to dig away the given wall, replacing it with floor.
// Returns whether the dig succeeded.
bool dig(Coords pos, TileType digging_power, bool can_splash)
{
	if (TILE(pos).type < digging_power)
		return false;

	Tile *wall = &TILE(pos);

	if (can_splash && (wall->type & 8))
		for (Coords d: plus_shape)
			if (!IS_DOOR(pos + d))
				dig(pos + d, max(digging_power, STONE), false);

	bool is_door = IS_DOOR(pos);
	wall->type &= FIRE | ICE;

	if (is_door)
		for (Coords d: plus_shape)
			dig(pos + d, DOOR, false);

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
	case MONKEY_1 ... TELE_MONKEY:
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
		if (forced_move(&player, d))
			move(attacker, attacker->pos + d);
		else
			damage(&player, 1, d, DMG_NORMAL);
		break;
	case BOMBER:
		explosion(attacker, Coords {});
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
	TileType digging_power = m->confusion ? NONE : m->digging_power;
	if (!m->aggro && digging_power == SHOP) {
		for (Coords d: plus_shape) {
			dig(m->pos + d, SHOP, false);
			damage(&MONSTER(m->pos + d), 4, d, DMG_NORMAL);
		}
		return MOVE_SPECIAL;
	}

	// Digging
	if ((m->type == MINOTAUR_1 || m->type == MINOTAUR_2) && m->state)
		digging_power = CATACOMB;
	if (dig(m->pos + dir, digging_power, m->type == DIGGER))
		return MOVE_SPECIAL;

	m->delay = 0;
	return MOVE_FAIL;
}

// Marks tiles that the player can see as “revealed”. Uses a shadow-casting algorithm.
void update_fov(void)
{
	Tile *tile = &TILE(player.pos);
	tile->revealed = true;
	update_fov_octant(tile, +1, +ARRAY_SIZE(g.board));
	update_fov_octant(tile, +1, -ARRAY_SIZE(g.board));
	update_fov_octant(tile, -1, +ARRAY_SIZE(g.board));
	update_fov_octant(tile, -1, -ARRAY_SIZE(g.board));
	update_fov_octant(tile, +ARRAY_SIZE(g.board), +1);
	update_fov_octant(tile, -ARRAY_SIZE(g.board), +1);
	update_fov_octant(tile, +ARRAY_SIZE(g.board), -1);
	update_fov_octant(tile, -ARRAY_SIZE(g.board), -1);
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
	case MONKEY_1 ... TELE_MONKEY:
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
		return;
	case BOMBER:
		monster_new(BOMB, m->pos, 3);
		break;
	case WATER_BALL:
		tile_change(m->pos, WATER);
		break;
	case GORGON_1:
	case GORGON_2:
		monster_spawn(m->type + STONE_STATUE - GORGON_1, m->pos, 0);
		return;
	case SARCO_1 ... SARCO_3:
	case DIREBAT_1 ... METROGNOME_2:
		--g.locking_enemies;
		if (m->type == NIGHTMARE_1 || m->type == NIGHTMARE_2)
			g.nightmare = 0;
		break;
	case PLAYER:
		longjmp(player_died, true);
	}

	TILE(m->pos).monster = 0;
}

// Spawns three skeletons in a line.
static void skull_spawn(const Monster *skull, Coords spawn_dir, Coords dir)
{
	u8 spawn_type = skull->type - SKULL_1 + SKELETON_1;
	for (i8 i = -1; i <= 1; ++i) {
		Coords pos = skull->pos + spawn_dir * i;
		dig(pos, SHOP, false);
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
		explosion(m, Coords {});
		return false;
	case WIND_STATUE:
	case BOMB_STATUE:
		if (type == DMG_BOMB)
			break;
		knockback(m, dir, m->state ? 2 : 0);
		return false;
	case CRATE:
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
	case MIMIC_1 ... SHOP_MIMIC:
	case SHRINE:
		if (type == DMG_BOMB || m->state == 2)
			break;
		return true;
	case MOLE:
	case GHOST:
		if (m->state == 1)
			break;
		return true;
	case ORB_1 ... ORB_3:
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
	case RIDER_1 ... RIDER_3:
		monster_transform(m, m->type - RIDER_1 + SKELETANK_1);
		m->flying = m->untrapped = false;
		knockback(m, dir, 1);
		return false;
	case SKELETANK_1 ... SKELETANK_3:
		if (dir != -m->dir)
			break;
		if (dmg >= proto[m->type].hp)
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
		tile_change(m->pos, m->type == FIRE_BEETLE ? FIRE : ICE);
		for (Coords d: plus_shape)
			tile_change(m->pos + d, m->type == FIRE_BEETLE ? FIRE : ICE);
		return false;
	case PIXIE:
	case BOMBSHROOM_:
		explosion(m, Coords {});
		return false;
	case GOOLEM:
		if (type == DMG_WEAPON && m->state == 0) {
			m->state = 1;
			tile_change(player.pos, OOZE);
		}
		break;
	case ORC_1 ... ORC_3:
		if (dir != -m->dir)
			break;
		knockback(m, dir, 1);
		return false;
	case SKULL_1 ... SKULL_3:
		monster_kill(m, DMG_NORMAL);
		skull_spawn(m, { dir.x == 0, dir.x != 0 }, -cardinal(dir));
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
		monster_transform(m, HEADLESS_2 + (m->type == SKELETON_3 || m->type == SKELETANK_3));
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
		MONSTER(g.stairs).hp = 0;
		move(m, g.stairs);
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
	g.player_moved = true;

	if (g.feet == BOOTS_LUNGING && g.boots_on) {
		i64 steps = 4;
		while (--steps && !forced && can_move(&player, dir))
			move(&player, player.pos + dir);
		Monster *in_my_way = &MONSTER(player.pos + dir);
		if (steps && damage(in_my_way, 4, dir, DMG_NORMAL))
			knockback(in_my_way, dir, 1);
	} else if (g.feet == BOOTS_LEAPING) {
		if (g.boots_on && can_move(&player, dir))
			move(&player, player.pos + dir);
		if (IS_BOGGED(&player))
			player.untrapped = true;
	}

	if (g.head == HEAD_MINERS) {
		TileType max_dig = TILE(player.pos).type == OOZE ? DOOR : player.digging_power;
		for (Coords d: plus_shape)
			dig(player.pos + d, max_dig, true);
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
	assert(m != &player || dir != Coords {});
	if (m->freeze || unbog(m))
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
	Coords arcs[] = {
		{ dir.x, dir.y },
		{ dir.x + dir.y, dir.y - dir.x },
		{ dir.x - dir.y, dir.y + dir.x },
		{ dir.y, -dir.x },
		{ -dir.y, dir.x },
		{ dir.y - dir.x, -dir.y - dir.x },
		{ -dir.y - dir.x, dir.x - dir.y },
		{ -dir.x, -dir.y },
	};

	MONSTER(pos).electrified = true;
	for (i64 i = 0; queue[i].x; ++i) {
		for (Coords arc: arcs) {
			Monster *m = &MONSTER(queue[i] + arc);
			if (m->electrified || m == &player)
				continue;
			m->electrified = true;
			damage(m, 1, cardinal(arc), DMG_NORMAL);
			queue[queue_length++] = queue[i] + arc;
		}
	}

	animation(ELECTRICITY, pos, dir);
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

	if (player.confusion)
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
		TileType max_dig = tile->type == OOZE ? DOOR : player.digging_power;
		dig(player.pos + dir, max_dig, true);
	} else if (TILE(dest).monster) {
		damage(&MONSTER(dest), dmg, dir, DMG_WEAPON);
		if (IS_WIRE(player.pos))
			chain_lightning(dest, dir);
	} else {
		move(&player, player.pos + dir);
		after_move(dir, false);
	}
}

static Item GameState::*const item_slot[] = {
#define X(name, slot, friendly, glyph, power) &GameState::slot,
#include "items.h"
#undef X
};

static u8 item_power[] = {
#define X(name, slot, friendly, glyph, power) power,
#include "items.h"
#undef X
};

// Adds an item to the player’s inventory.
// Returns the item the player had in that slot.
Item pickup_item(Item item)
{
	Item swapped_out;

	switch (item) {
	case HEART2:
	case HEART3:
		player.hp += item_power[item];
		return NO_ITEM;
	case BOMB_1:
	case BOMB_3:
		g.bombs += item_power[item];
		return NO_ITEM;
	default:
		swapped_out = g.*item_slot[item];
		g.*item_slot[item] = item;
	}

	player.digging_power = (TileType) (item_power[g.shovel] - item_power[g.head]);
	player.damage = (i8) item_power[g.weapon];
	player.radius = g.head == HEAD_MINERS ? 5 : 2;
	return swapped_out;
}

static void player_turn(char input)
{
	g.player_moved = false;

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
		forced_move(&player, direction(player.pos - player.prev_pos));
}

bool shadowed(Coords pos) {
	return g.nightmare && L2(pos - g.monsters[g.nightmare].pos) <
		(g.monsters[g.nightmare].type == NIGHTMARE_2 ? 12 : 6);
}

static bool check_aggro(Monster *m, Coords d, bool bomb_exploded)
{
	m->aggro = (d.y >= -5 && d.y <= 6)
		&& (d.x >= -9 && d.x <= 10)
		&& TILE(m->pos).revealed
		&& (TILE(m->pos).light >= 7777
			|| shadowed(m->pos)
			|| (m->type >= DIREBAT_1 && m->type <= METROGNOME_2)
			|| L2(player.pos - m->pos) < 8);

	if (m->aggro && (m->type == BLUE_DRAGON || m->type == EARTH_DRAGON))
		return true;

	// The nightmare-bomb-aggro bug
	if (m->aggro && (bomb_exploded || shadowed(m->pos)) && m->radius)
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
		animation(BOUNCE_TRAP, m->pos, {});
		forced_move(m, direction(m->pos - m->prev_pos));
		break;
	case BOUNCE:
		animation(BOUNCE_TRAP, m->pos, {});
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
	case SCATTER_TRAP:
		if (m == &player)
			monster_kill(m, DMG_NORMAL);
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
	for (Coords d: plus_shape) {
		Monster *m = &MONSTER(player.pos + d);
		if (m->type == FIRE_BEETLE || m->type == ICE_BEETLE) {
			tile_change(m->pos, m->type == FIRE_BEETLE ? FIRE : ICE);
			m->type = BEETLE;
		}
	}
}

// Runs one full beat of the game.
// During each beat, the player acts first, enemies second and traps last.
// Enemies act in decreasing priority order. Traps have an arbitrary order.
bool do_beat(char input)
{
	// Player’s turn
	g.input[g.current_beat++ & 31] = input;

	if (setjmp(player_died))
		return true;

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
		if (!monster_ai[m->type] || !m->hp)
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
		monster_ai[m->type](m, player.pos - m->pos);

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
		&& can_move(&player, direction(player.pos - player.prev_pos));

	if (!g.player_moved && TILE(player.pos).type == FIRE)
		damage(&player, 2, Coords {}, DMG_NORMAL);

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
	if (g.monsters[g.monkeyed].type == CONF_MONKEY)
		player.confusion = (u8) max(player.confusion, 1);

	return false;
}
