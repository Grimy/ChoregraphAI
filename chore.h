// chore.h - global types and vars definitions

#define player (g._player)
#define TILE(pos) (g.board[(pos).x][(pos).y])
#define BLOCKS_MOVEMENT(pos) (TILE(pos).class == WALL)
#define RNG() ((g.seed = g.seed >> 2 ^ g.seed << 3 ^ g.seed >> 14) & 3)

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
	GREEN_SLIME, BLUE_SLIME, YOLO_SLIME,
	SKELETON_1, SKELETON_2, SKELETON_3,
	BLUE_BAT, RED_BAT, GREEN_BAT,
	MONKEY_1, MONKEY_2,
	GHOST,
	ZOMBIE,
	WRAITH,
	MIMIC_1, MIMIC_2,
	HEADLESS, // a decapitated skeleton

	// Z2 enemies
	SKELETANK_1 = 100, SKELETANK_2, SKELETANK_3,
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
	FIRE_SLIME = 200, ICE_SLIME,
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
	BOMBER = 44,
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
	WIND_STATUE, SEEK_STATUE, BOMB_STATUE, MINE_STATUE,
	CRATE_1, CRATE_2, BARREL, TEH_URN,
	FREE_SPIDER, // a spider that isn’t inside a wall
	FIREPIG, // a fire trap (they’re treated as monsters, not traps)

	// Minibosses
	DIREBAT_1 = 144, DIREBAT_2,
	DRAGON, RED_DRAGON, BLUE_DRAGON,
	BANSHEE_1, BANSHEE_2,
	MINOTAUR_1, MINOTAUR_2,
	NIGHTMARE_1, NIGHTMARE_2,
	MOMMY, OGRE,

	// Other
	SHOPKEEPER = 88,
	PLAYER,
	BOMB,
} MonsterClass;

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
	MonsterClass class;
	i8 hp;
	Coords pos;
	Coords prev_pos;
	unsigned delay: 4;
	unsigned confusion: 4;
	unsigned freeze: 3;
	unsigned state: 2;
	bool aggro: 1;
	bool vertical: 1;
	bool untrapped: 1;
} Monster;

typedef struct {
	Monster *monster;
	TileClass class;
	i8 hp;
	i16 light;
	i8 zone;
	bool torch: 1;
	bool traps_destroyed: 1;
	bool revealed: 1;
	bool: 21;
} Tile;

typedef struct {
	TrapClass class;
	Coords pos;
	Coords dir;
} Trap;

// Properties that are common to all monsters of a type.
// This avoids duplicating information between all monsters of the same type.
typedef struct {
	i8 max_hp;
	u8 beat_delay;
	u8 radius;
	bool flying: 1;
	signed digging_power: 7;
	u32 priority;
	char *glyph;
	void (*act) (Monster*, Coords);
} ClassInfos;

static const ClassInfos class_infos[256];

// Gets the ClassInfos entry of the given monster’s class
#define CLASS(m) (class_infos[(m)->class])

typedef struct {
	Tile board[41][41];
	Monster _player;
	Monster monsters[64];
	Trap traps[64];

	u64 seed;
	bool player_moved;
	bool bomb_exploded;
	bool sliding_on_ice;
	bool boots_on;
	bool sarco_on;
	i32 player_bombs;
	i64 dragon_exhausted;
	i32 miniboss_killed;
	i32 sarcophagus_killed;
	i32 harpies_killed;
	i32 current_beat;
} GameState;

__extension__ static GameState g = {
	.board = {[0 ... 40] = {[0 ... 40] = {.class = WALL, .hp = 5}}},
	._player = {.class = PLAYER, .hp = 1},
	.player_bombs = 3,
	.boots_on = true,
	.dragon_exhausted = 4,
	.seed = 42,
};

static Coords spawn;
static Coords stairs;
static Monster *nightmare;
static Monster *last_monster = g.monsters;

// Some pre-declarations
static void do_beat(u8 input);
static bool damage(Monster *m, i64 dmg, Coords dir, DamageType type);
static bool forced_move(Monster *m, Coords offset);
