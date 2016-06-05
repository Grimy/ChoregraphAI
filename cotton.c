// cotton.c - core game logic

#define LENGTH(array) ((long) (sizeof(array) / sizeof(*(array))))
#define SIGN(x) (((x) > 0) - ((x) < 0))
#define DIRECTION(pos) ((Coords) {SIGN((pos).x), SIGN((pos).y)})
#define ABS(x) ((x) < 0 ? -(x) : (x))

#define L1(pos) (ABS((pos).x) + ABS((pos).y))
#define L2(pos) ((pos).x * (pos).x + (pos).y * (pos).y)

#define TILE(pos) (board[(pos).y][(pos).x])
#define IS_ENEMY(m) ((m) && (m) != &player)
#define IS_OPAQUE(pos) (TILE(pos).class == WALL)
#define CLASS(m) (class_infos[(m)->class])

static void damage(Monster *m, long dmg, bool bomblike);

// Moves the given monster to a specific position.
// Keeps track of the monster’s previous position.
static void move(Monster *m, Coords dest) {
	TILE(m->pos).monster = NULL;
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

static bool dig(Tile *wall, int digging_power, bool z4) {
	if (wall->hp > digging_power)
		return false;
	if (z4 && (wall->class != WALL || wall->hp == 0 || wall->hp > 2))
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

static void monster_remove(Monster *m) {
	if (m == &player)
		exit(0);
	TILE(m->pos).monster = NULL;
	Monster *prev = &player;
	while (prev->next != m)
		prev = prev->next;
	prev->next = m->next;
}

static void enemy_attack(Monster *attacker) {
	if (attacker->class == CONF_MONKEY) {
		monster_remove(attacker);
		player.confused = 4;
	} else if (attacker->class == PIXIE) {
		monster_remove(attacker);
	} else {
		damage(&player, 1, false);
	}
}

// Performs a movement action on behalf of an enemy.
// This includes attacking the player if they block the movement.
static bool enemy_move(Monster *m, Coords offset) {
	Tile *dest = &TILE(m->pos + offset);
	bool success = true;
	if (dest->monster == &player)
		enemy_attack(m);
	else if ((success = can_move(m, offset)))
		move(m, m->pos + offset);
	else if (dest->class == WALL)
		success = dig(dest, CLASS(m).dig, false);
	if (success)
		m->delay = CLASS(m).beat_delay;
	return success;
}

// Moves something by force, as caused by bounce traps, wind mages and knockback.
// Unlike enemy_move, this ignores confusion, doesn’t change the beat delay,
// and doesn’t cause digging.
static void forced_move(Monster *m, Coords offset) {
	Tile *dest = &TILE(m->pos + offset);
	if (dest->monster == &player)
		enemy_attack(m);
	else if (!dest->monster && dest->class != WALL)
		move(m, m->pos + offset);
}

// Checks whether the straight line from the player to the given coordinates
// is free from obstacles.
// This uses fractional coordinates: the center of tile (y, x) is at (y + 0.5, x + 0.5).
static bool los(double x, double y) {
	double dx = player.pos.x - x;
	double dy = player.pos.y - y;
	Coords c = {(int8_t) (x + .5), (int8_t) (y + .5)};
	if ((player.pos.x > x || x > c.x) &&
		dy * (c.y - y) > 0 &&
		IS_OPAQUE(c))
		return false;
	while (c.x != player.pos.x || c.y != player.pos.y) {
		double err_x = ABS((c.x + SIGN(dx) - x) * dy - (c.y - y) * dx);
		double err_y = ABS((c.x - x) * dy - (c.y + SIGN(dy) - y) * dx);
		int8_t old_cx = c.x;
		if (err_x < err_y + .001) {
			c.x += SIGN(dx);
			if (IS_OPAQUE(c))
				return false;
		}
		if (err_y < err_x + .001) {
			c.y += SIGN(dy);
			if (IS_OPAQUE(c) || IS_OPAQUE(((Coords) {old_cx, c.y})))
				return false;
		}
	}
	return true;
}

// Tests whether the player can see the tile at the given coordinates.
// This is true if there’s an unblocked line from the center of the player’s
// tile to any corner or the center of the destination tile.
static bool can_see(long x, long y) {
	Coords pos = player.pos;
	if (x < pos.x - 10 || x > pos.x + 9 || y < pos.y - 5 || y > pos.y + 5)
		return false;
	return los(x - .55, y - .55)
		|| los(x + .55, y - .55)
		|| los(x - .55, y + .55)
		|| los(x + .55, y + .55)
		|| los(x, y);
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
	forced_move(m, (Coords) {SIGN(m->pos.x - player.pos.x), SIGN(m->pos.y - player.pos.y)});
	m->delay = 1;
}

static void bomb_plant(Coords pos, uint8_t delay) {
	Monster bomb = {.class = BOMB, .pos = pos, .next = player.next, .aggro = true, .delay = delay};
	player.next = &monsters[monster_count];
	monsters[monster_count++] = bomb;
}

static void kill(Monster *m, bool bomblike) {
	monster_remove(m);
	Tile *tile = &TILE(m->pos);
	if (!bomblike && (m->class == WARLOCK_1 || m->class == WARLOCK_2))
		move(&player, m->pos);
	else if (m->class == ICE_SLIME || m->class == YETI)
		tile->class = tile->class == FIRE ? WATER : ICE;
	else if (m->class == FIRE_SLIME || m->class == HELLHOUND)
		tile->class = tile->class == ICE ? WATER : tile->class == WATER ? FLOOR : FIRE;
	else if (m->class == BOMBER)
		bomb_plant(m->pos, 3);
}

// Deals damage to the given monster.
static void damage(Monster *m, long dmg, bool bomblike) {
	if (!bomblike && (m->class == BLADENOVICE || m->class == BLADEMASTER) && m->state < 2) {
		knockback(m);
		m->state = 1;
		return;
	}
	if ((m->class == TARMONSTER || m->class == WALL_MIMIC ||
			m->class == FIRE_MIMIC || m->class == ICE_MIMIC) && m->state < 2)
		return;
	m->hp -= dmg;
	if (m->hp <= 0)
		kill(m, bomblike);
	else if (CLASS(m).beat_delay == 0)
		knockback(m);
}

static void player_attack(Monster *m) {
	if (TILE(player.pos).class == OOZE)
		return;
	if (m->class == OOZE_GOLEM)
		TILE(player.pos).class = OOZE;
	damage(m, 1, false);
}

static void player_move(Coords offset) {
	if (player.confused)
		offset = -offset;
	Tile *dest = &TILE(player.pos + offset);
	player.prev_pos = player.pos;
	if (dest->class == WALL)
		dig(dest, TILE(player.pos).class == OOZE ? 0 : 2, false);
	else if (IS_ENEMY(dest->monster))
		player_attack(dest->monster);
	else
		move(&player, player.pos + offset);
}
