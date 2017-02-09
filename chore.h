// chore.h - global types and vars definitions

#include "base.h"

#define player (g.monsters[1])

#define TILE(pos) (g.board[(pos).x][(pos).y])
#define MONSTER(pos) (g.monsters[TILE(pos).monster])
#define IS_WIRE(pos) (TILE(pos).wired && !TILE(pos).destroyed)

#define BLOCKS_LOS(pos) (TILE(pos).type >> 7)
#define IS_EMPTY(pos) (!BLOCKS_LOS(pos) && !TILE(pos).monster)
#define IS_DIGGABLE(pos) (TILE(pos).type >= DOOR)
#define IS_DOOR(pos) (TILE(pos).type == DOOR)

#define RNG() ((g.seed = g.seed >> 2 ^ g.seed << 3 ^ g.seed >> 14) & 3)
#define IS_BOGGED(m) (!(m)->untrapped \
	&& (TILE((m)->pos).type == WATER \
	|| (TILE((m)->pos).type == TAR)))

// Gets the TypeInfos entry of the given monster’s type.
#define TYPE(m) (type_infos[(m)->type])

// A pair of cartesian coordinates. Each variable of this type is either:
// * A point, representing an absolute position within the grid (usually named `pos`)
// * A vector, representing the relative position of another entity (usually named `d`)
// * A unit vector, representing a direction of movement (usually named `dir` or `move`)
struct Coords {
	i8 x;
	i8 y;

	void operator+=(const Coords& that)                  { x += that.x; y += that.y; }
	constexpr Coords operator-()                   const { return { -x, -y }; }
	constexpr Coords operator+(const Coords &that) const { return { x + that.x, y + that.y }; }
	constexpr Coords operator-(const Coords &that) const { return { x - that.x, y - that.y }; }
	constexpr Coords operator*(i8 scalar)          const { return { x * scalar, y * scalar }; }
	constexpr Coords operator/(i8 scalar)          const { return { x / scalar, y / scalar }; }
	constexpr bool operator==(const Coords &that)  const { return x == that.x && y == that.y; }
	constexpr bool operator!=(const Coords &that)  const { return x != that.x || y != that.y; }
};

// Converts relative coordinates to a direction.
// For example, {5, -7} becomes {1, -1}.
#define DIRECTION(d) ((Coords) {SIGN((d).x), SIGN((d).y)})

// Converts relative coordinates to a *cardinal* direction.
// Like DIRECTION, but diagonals are turned into horizontals.
#define CARDINAL(d) ((Coords) {SIGN((d).x), (d).x ? 0 : SIGN((d).y)})

// L¹ norm of a vector (= number of beats it takes to move to this relative position).
#define L1(d) (abs((d).x) + abs((d).y))

// *Square* of the L² norm of a vector (mostly used for aggro/lighting).
#define L2(d) ((d).x * (d).x + (d).y * (d).y)

// Playable characters.
enum CharId {
	CADENCE,
	MELODY,
	ARIA,     // <3
	DORIAN,
	ELI,
	MONK,
	DOVE,
	CODA,
	BOLT,
	BARD,
	NOCTURNA,
};

enum MonsterType : u8 {
	NO_MONSTER,

	// Z1 enemies
	GREEN_SLIME, BLUE_SLIME, YELLOW_SLIME,
	SKELETON_1, SKELETON_2, SKELETON_3,
	BLUE_BAT, RED_BAT, GREEN_BAT,
	MONKEY_1, MONKEY_2,
	GHOST,
	ZOMBIE,
	WRAITH,
	MIMIC_1, MIMIC_2, WHITE_MIMIC,

	// Z2 enemies
	SKELETANK_1, SKELETANK_2, SKELETANK_3,
	WINDMAGE_1, WINDMAGE_2, WINDMAGE_3,
	MUSHROOM_1, MUSHROOM_2,
	GOLEM_1, GOLEM_2,
	ARMADILLO_1, ARMADILLO_2,
	CLONE,
	TARMONSTER,
	MOLE,
	WIGHT,
	WALL_MIMIC,
	LIGHTSHROOM, BOMBSHROOM,
	BOMBSHROOM_, // an “activated” explosive mushroom

	// Z3 enemies
	FIRE_SLIME, ICE_SLIME,
	RIDER_1, RIDER_2, RIDER_3,
	EFREET, DJINN,
	ASSASSIN_1, ASSASSIN_2,
	FIRE_BEETLE, ICE_BEETLE,
	HELLHOUND,
	SHOVE_1,
	YETI,
	GHAST,
	FIRE_MIMIC, ICE_MIMIC,
	FIRE_POT, ICE_POT,
	SHOVE_2,
	BEETLE, // a fire/ice beetle that has shed its armor

	// Z4 enemies
	BOMBER,
	DIGGER,
	BLACK_BAT,
	ARMADILDO,
	BLADENOVICE, BLADEMASTER,
	GHOUL,
	GOOLEM,
	HARPY,
	LICH_1, LICH_2, LICH_3,
	CONF_MONKEY, TELE_MONKEY,
	PIXIE,
	SARCO_1, SARCO_2, SARCO_3,
	SPIDER,
	WARLOCK_1, WARLOCK_2,
	MUMMY,
	WIND_STATUE, MIMIC_STATUE, BOMB_STATUE, MINE_STATUE,
	CRATE_1, CRATE_2, BARREL, TEH_URN,
	FREE_SPIDER, // a spider that isn’t inside a wall
	FIREPIG, // a fire trap (they’re treated as monsters, not traps)

	// Z5 enemies
	CRYSTAL_1,
	SKULL_1,
	WATER_BALL,
	CENSORED,
	ELECTRO_1, ELECTRO_2, ELECTRO_3,
	ORB_1, ORB_2, ORB_3,
	GORGON_1,
	WIRE_ZOMBIE,
	SKULL_2, SKULL_3,
	CRYSTAL_2, CRYSTAL_3, CRYSTAL_4,
	EVIL_EYE_1,
	GORGON_2,
	EVIL_EYE_2,
	ORC_1, ORC_2, ORC_3,
	DEVIL_1, DEVIL_2,
	PURPLE_SLIME,
	CURSE,
	MIMIC_3, SHOP_MIMIC,
	BLACK_SLIME, WHITE_SLIME,
	MIMIC_4, MIMIC_5,
	TAR_BALL,
	STONE_STATUE,
	GOLD_STATUE,

	// Minibosses
	DIREBAT_1, DIREBAT_2,
	DRAGON, RED_DRAGON, BLUE_DRAGON,
	BANSHEE_1, BANSHEE_2,
	MINOTAUR_1, MINOTAUR_2,
	NIGHTMARE_1, NIGHTMARE_2,
	MOMMY, OGRE,
	METROGNOME_1, METROGNOME_2,
	EARTH_DRAGON,

	// Other
	SHOPKEEPER,
	PLAYER,
	BOMB,
	CHEST,
	SHRINE,
	HEADLESS, // a decapitated skeleton
};

enum ItemType : u8 {
	NO_ITEM,
	BOMBS,
	BOMBS_3,
	HEART_1,
	HEART_2,
	JEWELED,
	LUNGING,
	MEMERS_CAP,
	PACEMAKER,
	SCROLL_FREEZE,
	ITEM_LAST,
};

enum TileType : u8 {
	FLOOR = 0,
	SHOP_FLOOR = 3,
	WATER = 4,
	TAR = 8,
	STAIRS = 9,
	FIRE = 10,
	ICE = 11,
	OOZE = 17,
	WIRE = 20,

	EDGE = 128,
	DOOR = 129,
	WALL = 130,
	Z4WALL = 133,
	FIREWALL = 138,
	ICEWALL = 139,
};

enum TrapType {
	OMNIBOUNCE,
	BOUNCE,      // any of the eight directional bounce traps
	SPIKE,
	TRAPDOOR,
	CONFUSE,
	TELEPORT,
	TEMPO_DOWN,
	TEMPO_UP,
	RAND_BOUNCE, // not implemented
	BOMBTRAP,
};

// When an enemy attempts a movement, it can either:
enum MoveResult {
	MOVE_FAIL,     // bump in a wall or other monster
	MOVE_SPECIAL,  // crawl out of water/tar, or dig a wall
	MOVE_ATTACK,   // attack the player
	MOVE_SUCCESS,  // actually change position
};

// Each damage has a type. It determines which on-damage triggers are in effect.
enum DamageType {
	DMG_NORMAL,
	DMG_WEAPON,
	DMG_BOMB,
	// Piercing and phasing damage aren’t implemented at the moment
	// DMG_PIERCING,
	// DMG_PHASING,
};

// A “Monster” can be either an enemy, a bomb, or the player (yes, we are all monsters).
struct Monster {
	i8 damage;
	i8 hp;
	u8 max_delay;
	bool flying;
	i16 radius;
	i8 digging_power;
	u8 priority;

	Coords pos;
	Coords prev_pos;
	Coords dir;
	u8 type;
	u8 item;

	u8 delay;
	u8 confusion;
	u8 freeze;
	u8 state;
	u8 exhausted;
	bool aggro;
	bool untrapped;
	bool electrified: 1;
	bool knocked: 1;
	bool requeued: 1;
	bool was_requeued: 1;
	bool: 4;
};

// Properties that are common to all monsters of a type.
// This avoids duplicating information between all monsters of the same type.
struct TypeInfos {
	i8 damage;
	i8 max_hp;
	u8 max_delay;
	bool flying;
	i16 radius;
	i8 digging_power;
	u8 priority;
	const char *glyph;
	void (*act) (Monster*, Coords);
};

struct Tile {
	u8 type;
	i8 hp;
	u8 monster;
	u8 item;
	u16 light;
	bool revealed;
	bool torch: 1;
	bool destroyed: 1;
	bool wired: 1;
	i8 padding: 5;
};

struct Trap {
	u32 type;
	Coords pos;
	Coords dir;
};

typedef struct {
	// Contents of the level
	Tile board[32][32];
	Monster monsters[72];
	Trap traps[32];

	// Global properties
	u64 seed;
	u8 input[32];
	u8 current_beat;
	u8 locking_enemies;
	u8 nightmare;
	u8 monkeyed;
	u8 mommy_spawn;
	u8 sarco_spawn;
	u8 last_monster;

	// Attributes specific to the player
	u8 inventory[ITEM_LAST];
	bool player_moved;
	bool sliding_on_ice;
	bool boots_on;
	u8 iframes;
	u64: 24;
} GameState;

extern const TypeInfos type_infos[];
extern Coords spawn;
extern Coords stairs;
extern u64 character;
extern const Coords plus_shape[];
extern thread_local GameState g;

Monster* monster_spawn(u8 type, Coords pos, u8 delay);
void xml_parse(i32 argc, char **argv);
bool do_beat(u8 input);
void destroy_wall(Coords pos);
bool damage(Monster *m, i64 dmg, Coords dir, DamageType type);
void cast_light(Tile *tile, i64 x, i64 y);
void update_fov(void);
u8 pickup_item(u8 item);
void adjust_lights(Coords pos, i8 diff, double radius);
void bomb_detonate(Monster *m, Coords d);
void fireball(Coords pos, i8 dir);
void cone_of_cold(Coords pos, i8 dir);
bool can_move(Monster *m, Coords dir);
MoveResult enemy_move(Monster *m, Coords dir);
void monster_kill(Monster *m, DamageType type);
bool forced_move(Monster *m, Coords offset);
void tile_change(Coords pos, u8 new_type);
