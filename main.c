// main.c - core game logic

#include "chore.h"

const Coords plus_shape[] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}, {0, 0}};
Coords spawn;
Coords stairs;

__extension__ __thread GameState g = {
	.board = {[0 ... 31] = {[0 ... 31] = {.class = WALL, .hp = 5}}},
	.inventory = {[BOMBS] = 3},
	.boots_on = true,
};

// Some pre-declarations
static bool damage(Monster *m, i64 dmg, Coords dir, DamageType type);

void monster_init(Monster *new, MonsterClass type, Coords pos)
{
	new->class = type;
	new->pos = new->prev_pos = pos;
	new->hp = CLASS(new).max_hp;
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
	Tile dest = TILE(m->pos + dir);
	if (dest.monster)
		return &MONSTER(m->pos + dir) == &player;
	if (m->class == SPIDER)
		return IS_WALL(m->pos + dir) && !dest.torch;
	if (m->class == MOLE && (dest.class == WATER || dest.class == TAR))
		return false;
	return dest.class != WALL;
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
	assert(IS_WALL(pos));
	Tile *wall = &TILE(pos);

	wall->class =
		wall->hp == 2 && wall->zone == 2 ? FIRE :
		wall->hp == 2 && wall->zone == 3 ? ICE :
		FLOOR;
	if (MONSTER(pos).class == SPIDER) {
		MONSTER(pos).class = FREE_SPIDER;
		MONSTER(pos).delay = 1;
	}
	if (wall->torch)
		adjust_lights(pos, -1, 0);
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

void damage_tile(Coords pos, Coords origin, i64 dmg, DamageType type)
{
	if (IS_WALL(pos))
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
		tile->traps_destroyed = true;
		tile->class = tile->class == WATER ? FLOOR : tile->class == ICE ? WATER : tile->class;
		damage_tile(this->pos + square_shape[i], this->pos, 4, DMG_BOMB);
	}
	this->hp = 0;
	g.bomb_exploded = true;
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
		g.monkey = attacker;
		TILE(attacker->pos).monster = 0;
		attacker->hp *= attacker->class == MONKEY_2 ? 3 : 4;
		break;
	case PIXIE:
		TILE(attacker->pos).monster = 0;
		attacker->hp = 0;
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
	if (is_bogged(m))
		return MOVE_SPECIAL;
	if (m->confusion && m->class != BARREL)
		dir = -dir;

	// Attack
	if (&MONSTER(m->pos + dir) == &player) {
		enemy_attack(m);
		m->requeued = false;
		return MOVE_ATTACK;
	}

	// Actual movement
	if (can_move(m, dir)) {
		move(m, m->pos + dir);
		m->requeued = false;
		return MOVE_SUCCESS;
	}

	// Try the move again after other monsters have moved
	if (!m->requeued) {
		m->requeued = true;
		return MOVE_FAIL;
	}
	m->requeued = false;

	// Trampling
	i32 digging_power = m->confusion ? -1 : CLASS(m).digging_power;
	if (!m->aggro && digging_power == 4) {
		for (i64 i = 0; i < 4; ++i)
			damage_tile(m->pos + plus_shape[i], m->pos, 4, DMG_NORMAL);
		return MOVE_SPECIAL;
	}

	// Digging
	digging_power += (m->class == MINOTAUR_1 || m->class == MINOTAUR_2) && m->state;
	if (dig(m->pos + dir, digging_power, false)) {
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
	Monster *bomb = &player + 1;
	while (bomb->hp > 0)
		++bomb;
	assert(bomb->class == BOMB);
	bomb->hp = 1;
	bomb->pos = pos;
	bomb->delay = delay;
}

// Overrides a tile with a given floor hazard. Also destroys traps on the tile.
// Special cases: stairs are immune, fire+ice => water, fire+water => nothing.
void tile_change(Tile *tile, TileClass new_class)
{
	tile->class =
		tile->class == STAIRS ? STAIRS :
		tile->class * new_class == FIRE * ICE ? WATER :
		tile->class == WATER && new_class == FIRE ? FLOOR :
		new_class;
	tile->traps_destroyed = true;
}

// Kills the given monster, handling on-death effects.
void monster_kill(Monster *m, DamageType type)
{
	m->hp = 0;
	m->requeued = false;
	TILE(m->pos).monster = 0;

	switch (m->class) {
	case LIGHTSHROOM:
		adjust_lights(m->pos, -1, 3);
		break;
	case ICE_SLIME:
	case YETI:
	case ICE_POT:
	case ICE_MIMIC:
		tile_change(&TILE(m->pos), ICE);
		break;
	case FIRE_SLIME:
	case HELLHOUND:
	case FIRE_POT:
	case FIRE_MIMIC:
		tile_change(&TILE(m->pos), FIRE);
		break;
	case WARLOCK_1:
	case WARLOCK_2:
		if (type == DMG_WEAPON)
			move(&player, m->pos);
		break;
	case BOMBER:
		bomb_plant(m->pos, 3);
		break;
	case SARCO_1:
	case SARCO_2:
	case SARCO_3:
		g.sarcophagus_killed = true;
		break;
	case DIREBAT_1 ... OGRE:
		g.miniboss_killed = true;
		break;
	}
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
		if (L1(m->pos - player.pos) == 1) {
			knockback(m, dir, 1);
			m->state = 1;
		}
		return false;
	case RIDER_1:
	case RIDER_2:
	case RIDER_3:
		m->class += SKELETANK_1 - RIDER_1;
		knockback(m, dir, 1);
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
		m->prev_pos = m->pos - dir;
		return false;
	case ICE_BEETLE:
	case FIRE_BEETLE:
		knockback(m, dir, 1);
		TileClass hazard = m->class == FIRE_BEETLE ? FIRE : ICE;
		for (i64 i = 0; i < 5; ++i)
			tile_change(&TILE(m->pos + plus_shape[i]), hazard);
		return false;
	case PIXIE:
	case BOMBSHROOM_:
		bomb_detonate(m, NO_DIR);
		return false;
	case GOOLEM:
		tile_change(&TILE(player.pos), OOZE);
		break;
	case PLAYER:
		if (g.iframes)
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
	case BANSHEE_1:
	case BANSHEE_2:
		knockback(m, dir, 1);
		return false;
	case PLAYER:
		g.iframes = 2;
	}

	return true;
}

static void after_move(Coords dir)
{
	if (g.inventory[LUNGING] && g.boots_on) {
		i64 steps = 4;
		while (--steps && can_move(&player, dir))
			move(&player, player.pos + dir);
		if (steps && damage(&MONSTER(player.pos + dir), 4, dir, DMG_NORMAL))
			knockback(&MONSTER(player.pos + dir), dir, 1);
	}

	if (g.inventory[MEMERS_CAP]) {
		i32 digging_power = TILE(player.pos).class == OOZE ? 0 : 2;
		for (i64 i = 0; i < 4; ++i)
			dig(player.pos + plus_shape[i], digging_power, false);
	}
}

// Moves something by force (as caused by bounce traps, wind mages and knockback).
// Unlike enemy_move, ignores confusion, delay, and digging.
bool forced_move(Monster *m, Coords dir)
{
	assert(m != &player || L1(dir));
	if (m->freeze || is_bogged(m) || (m == &player && g.monkey))
		return false;

	if (&MONSTER(m->pos + dir) == &player) {
		enemy_attack(m);
		return true;
	} else if (IS_EMPTY(m->pos + dir)) {
		m->prev_pos = m->pos;
		move(m, m->pos + dir);
		if (m == &player)
			after_move(dir);
		return true;
	}

	return false;
}

// Attempts to move the player in the given direction
// Will trigger attacking/digging if the destination contains an enemy/a wall.
static void player_move(i8 x, i8 y)
{
	player.prev_pos = player.pos;
	Coords dir = {x, y};
	i32 dmg = TILE(player.pos).class == OOZE ? 0 :
		g.inventory[JEWELED] ? 5 : 1;

	if (player.confusion || (g.monkey && g.monkey->class == CONF_MONKEY))
		dir = -dir;

	if (g.monkey) {
		Monster *m = g.monkey;
		m->hp -= max(1, dmg);
		if (m->hp <= 0)
			g.monkey = NULL;
		if (m->class == TELE_MONKEY)
			monster_kill(&player, DMG_NORMAL);
		else if (m->class != CONF_MONKEY)
			return;
	}

	if (is_bogged(&player))
		return;

	Tile *dest = &TILE(player.pos + dir);

	if (dest->class == WALL) {
		dig(player.pos + dir, TILE(player.pos).class == OOZE ? 0 : 2, false);
	} else if (dest->monster) {
		damage(&MONSTER(player.pos + dir), dmg, dir, DMG_WEAPON);
	} else {
		g.player_moved = true;
		move(&player, player.pos + dir);
		after_move(dir);
	}
}

// Deals bomb-like damage to all monsters on a horizontal line).
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
	for (u64 i = 0; i < ARRAY_SIZE(cone_shape); ++i)
		MONSTER(pos + dir * cone_shape[i]).freeze = 5;
}

bool player_won()
{
	return TILE(player.pos).class == STAIRS
		&& g.miniboss_killed
		&& (TILE(player.pos).zone != 4 || g.sarcophagus_killed);
}

void pickup_item(ItemClass item)
{
	++g.inventory[item];
}

static void player_turn(u8 input)
{
	player.confusion -= SIGN(player.confusion);
	player.freeze -= SIGN(player.freeze);
	g.iframes -= SIGN(g.iframes);
	g.player_moved = false;

	// While frozen or ice-sliding, directional inputs are ignored
	if ((g.sliding_on_ice || player.freeze) && input < 4)
		input = 6;

	if (TILE(player.pos).item) {
		pickup_item(TILE(player.pos).item);
		TILE(player.pos).item = 0;
	}

	switch (input) {
	case 0:
		player_move(-1,  0);
		break;
	case 1:
		player_move( 0,  1);
		break;
	case 2:
		player_move( 1,  0);
		break;
	case 3:
		player_move( 0, -1);
		break;
	case 4:
		if (g.inventory[BOMBS]) {
			--g.inventory[BOMBS];
			bomb_plant(player.pos, 3);
		}
		break;
	case 5:
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

static void enemy_turn(Monster *m)
{
	Coords d = player.pos - m->pos;

	m->knocked = false;

	if (m->confusion)
		--m->confusion;

	if (!m->aggro) {
		bool shadowed = g.nightmare && L2(m->pos - g.monsters[g.nightmare].pos) < 9;
		m->aggro = TILE(m->pos).revealed
			&& (TILE(m->pos).light >= 102
				|| L2(player.pos - m->pos) < 9
				|| shadowed)
			&& (d.y >= -5 && d.y <= 6)
			&& (d.x >= -10 && d.x <= 9);
		if (m->aggro && (m->class == BLUE_DRAGON || g.bomb_exploded || shadowed)) {
			(void) 0;
		} else if (L2(d) <= CLASS(m).radius) {
			if (m->class >= SARCO_1 && m->class <= SARCO_3)
				m->delay = CLASS(m).beat_delay;
		} else {
			return;
		}
	}

	if (m->freeze)
		--m->freeze;
	else if (m->delay)
		--m->delay;
	else
		CLASS(m).act(m, d);
}

static void trap_turn(Trap *this)
{
	if (TILE(this->pos).traps_destroyed)
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
		if (!m->confusion)
			m->confusion = 10;
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

// Runs one full beat of the game.
// During each beat, the player acts first, enemies second and traps last.
// Enemies act in decreasing priority order. Traps have an arbitrary order.
void do_beat(u8 input)
{
	g.input[g.length++] = input;
	++g.current_beat;
	g.bomb_exploded = false;

	player_turn(input);
	if (player_won())
		return;
	update_fov();

	for (Monster *m = &player + 1; CLASS(m).act; ++m) {
		if (m->hp <= 0)
			continue;
		u8 old_state = m->state;
		enemy_turn(m);
		if (m->requeued)
			m->state = old_state;
	}

	for (Monster *m = &player + 1; CLASS(m).act; ++m)
		if (m->requeued)
			CLASS(m).act(m, player.pos - m->pos);

	for (Trap *t = g.traps; t->pos.x; ++t)
		trap_turn(t);
}
