// chore.h - global types and vars definitions

#include "base.h"

#define player (g.monsters[1])
#define confused confusion > g.current_beat
#define TILE(pos) (g.board[(pos).x][(pos).y])

#define MONSTER(pos) (g.monsters[TILE(pos).monster])
#define BLOCKS_LOS(pos) (TILE(pos).class == WALL)
#define IS_WALL(pos) (TILE(pos).class == WALL && TILE(pos).hp < 5)
#define IS_EMPTY(pos) (TILE(pos).class != WALL && !TILE(pos).monster)

#define RNG() ((g.seed = g.seed >> 2 ^ g.seed << 3 ^ g.seed >> 14) & 3)
#define STUCK(m) (!CLASS(m).flying && (TILE((m)->pos).class == WATER \
			|| (TILE((m)->pos).class == TAR && !(m)->untrapped)))

// Gets the ClassInfos entry of the given monster’s class
#define CLASS(m) (class_infos[(m)->class])

// A pair of cartesian coordinates. Each variable of this type is either:
// * A point, representing an absolute position within the grid (usually named `pos`)
// * A vector, representing the relative position of another entity (usually named `d`)
// * A unit vector, representing a direction of movement (usually named `dir` or `move`)
typedef i8 Coords __attribute__((ext_vector_type(2)));

// The direction of non-directional damage.
#define NO_DIR ((Coords) {0, 0})

// Converts relative coordinates to a direction.
// For example, {5, -7} becomes {1, -1}.
#define DIRECTION(d) ((Coords) {SIGN((d).x), SIGN((d).y)})

// Converts relative coordinates to a *cardinal* direction.
// Diagonals are turned into horizontals.
#define CARDINAL(d) ((Coords) {SIGN((d).x), (d).x ? 0 : SIGN((d).y)})

// L¹ norm of a vector (= number of beats it takes to move to this relative position).
#define L1(d) (abs((d).x) + abs((d).y))

// *Square* of the L² norm of a vector (mostly used for aggro/lighting).
#define L2(d) ((d).x * (d).x + (d).y * (d).y)

// Monster types.
typedef enum __attribute__((__packed__)) {
	// Z1 enemies
	GREEN_SLIME, BLUE_SLIME, YELLOW_SLIME,
	SKELETON_1, SKELETON_2, SKELETON_3,
	BLUE_BAT, RED_BAT, GREEN_BAT,
	MONKEY_1, MONKEY_2,
	GHOST,
	ZOMBIE,
	WRAITH,
	MIMIC_1, MIMIC_2,
	HEADLESS, // a decapitated skeleton

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
	CRATE_1, CRATE_2, BARREL, TEH_URN, CHEST,
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
	EYE_1,
	GORGON_2,
	EYE_2,
	ORC_1, ORC_2, ORC_3,
	DEVIL_1, DEVIL_2,
	PURPLE_SLIME,
	CURSE,
	MIMIC_3, SHOP_MIMIC,
	BLACK_SLIME, WHITE_SLIME,
	MIMIC_4, MIMIC_5,
	TAR_BALL,

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
} MonsterClass;

// Items
typedef enum __attribute__((__packed__)) {
	NO_ITEM,
	BOMBS,
	BOMBS_3,
	JEWELED,
	LUNGING,
	MEMERS_CAP,
	PACEMAKER,
	ITEM_LAST,
} ItemClass;

// Tile types.
typedef enum __attribute__((__packed__)) {
	WALL = 0,   // actually includes level edges and doors
	FLOOR = 1,
	SHOP = 3,   // shop floor (behaves like FLOOR, but looks different)
	WATER = 4,
	TAR = 8,
	STAIRS = 9,
	FIRE = 10,
	ICE = 11,
	OOZE = 17,
} TileClass;

// Trap types.
typedef enum {
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
} TrapClass;

// When a monster attempts a movement, it can either:
typedef enum {
	MOVE_FAIL,     // bump in a wall or other monster
	MOVE_SPECIAL,  // crawl out of water/tar, or dig a wall
	MOVE_ATTACK,   // attack another monster
	MOVE_SUCCESS,  // actually change position
} MoveResult;

// Each damage has a type. It determines which on-damage triggers are in effect.
typedef enum {
	DMG_NORMAL,
	DMG_WEAPON,
	DMG_BOMB,
	// Piercing and phasing damage aren’t implemented at the moment
	// DMG_PIERCING,
	// DMG_PHASING,
} DamageType;

// A “Monster” can be either an enemy, a bomb, or the player (yes, we are all monsters).
typedef struct {
	Coords pos;
	Coords prev_pos;
	MonsterClass class;
	i8 hp;
	u8 delay;
	u8 confusion;
	u8 freeze;
	u8 state;
	u8 exhausted;
	bool aggro;
	bool vertical;
	bool untrapped;
	bool requeued;
	bool was_requeued;
	bool knocked;
	ItemClass item;
} Monster;

typedef struct {
	TileClass class;
	i8 hp;
	u8 monster;
	ItemClass item;
	i16 light;
	bool revealed;
	bool torch: 1;
	bool traps_destroyed: 1;
	bool wired: 1;
	i8 zone: 5;
} Tile;

typedef struct {
	TrapClass class;
	Coords pos;
	Coords dir;
} Trap;

// Properties that are common to all monsters of a type.
// This avoids duplicating information between all monsters of the same type.
typedef struct {
	i8 damage;
	i8 max_hp;
	u8 beat_delay;
	u8: 8;
	u16 radius;
	bool flying;
	i8 digging_power;
	u64 priority;
	char *glyph;
	void (*act) (Monster*, Coords);
} ClassInfos;

typedef struct {
	// Contents of the level
	Tile board[32][32];
	Monster monsters[72];
	Trap traps[32];

	// Global properties
	u64 seed;
	u8 input[32];
	u8 current_beat;
	bool bomb_exploded;
	bool miniboss_killed;
	bool sarcophagus_killed;
	u8 nightmare;

	// Attributes specific to the player
	u8 inventory[ITEM_LAST];
	bool player_moved;
	bool sliding_on_ice;
	bool boots_on;
	u8 iframes;
	u8 monkey;
	u64 padding: 56;
} GameState;

extern const ClassInfos class_infos[256];
extern Coords spawn;
extern Coords stairs;
extern const Coords plus_shape[];
extern __thread GameState g;

void xml_parse(i32 argc, char **argv);
void do_beat(u8 input);
bool player_won(void);

void cast_light(Tile *tile, i64 x, i64 y);
void update_fov(void);
ItemClass pickup_item(ItemClass item);
void adjust_lights(Coords pos, i8 diff, i8 brightness);
void monster_init(Monster *new, MonsterClass type, Coords pos);
void bomb_detonate(Monster *this, Coords d);
void fireball(Coords pos, i8 dir);
void cone_of_cold(Coords pos, i8 dir);
void damage_tile(Coords pos, Coords origin, i64 dmg, DamageType type);
bool can_move(Monster *m, Coords dir);
MoveResult enemy_move(Monster *m, Coords dir);
void monster_kill(Monster *m, DamageType type);
bool forced_move(Monster *m, Coords offset);
void enemy_attack(Monster *attacker);
void tile_change(Coords pos, TileClass new_class);
