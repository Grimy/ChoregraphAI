// cotton.h - types and global vars definitions

// Exit codes
#define VICTORY 0
#define DEATH 254

// A pair of cartesian coordinates.
typedef i8 Coords __attribute__((ext_vector_type(2)));

// Human-friendly names for monster classes.
// Numerical values were arbitrarily picked to make parsing dungeon XML easier.
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
	HEADLESS,

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
	LIGHTSHROOM, BOMBSHROOM, BOMBSHROOM_,

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
	BEETLE,

	// Z4 enemies
	BOMBER = 44,
	DIGGER,
	BLACK_BAT,
	ARMADILDO,
	BLADENOVICE, BLADEMASTER,
	GHOUL,
	OOZE_GOLEM,
	HARPY,
	LICH_1, LICH_2, LICH_3,
	CONF_MONKEY, TELE_MONKEY,
	PIXIE,
	SARCO_1, SARCO_2, SARCO_3,
	SPIDER,
	WARLOCK_1, WARLOCK_2,
	MUMMY,
	WIND_STATUE, SEEK_STATUE, BOMB_STATUE, MINE_STATUE,
	CRATE_1, CRATE_2,
	FREE_SPIDER,

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

// Human-readable names for tile types.
// Note that WALL can be any wall type, including level edges and doors.
typedef enum {
	WALL = 0,
	FLOOR = 1,
	SHOP = 3,
	WATER = 4,
	TAR = 8,
	STAIRS = 9,
	FIRE = 10,
	ICE = 11,
	OOZE = 17,
} TileClass;

// Human-readable names for traps.
// BOUNCE includes all eight directional bounce traps, but not omni-bounce nor random-bounce.
typedef enum {
	OMNIBOUNCE,
	BOUNCE,
	SPIKE,
	TRAPDOOR,
	CONFUSE,
	TELEPORT,
	TEMPO_DOWN,
	TEMPO_UP,
	BOMBTRAP = 9,
	FIREPIG,
} TrapClass;

// A “Monster” is either an enemy or the player. Yes, we are all monsters.
// Honestly, “Entity” is way too generic, and “Character” sounds too much like “char*”.
typedef struct monster {
	struct monster *next;
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
	TileClass class;
	i8 hp;
	i8 torch;
	i8 zone;
	i8 revealed;
	Monster *monster;
} Tile;

typedef struct {
	TrapClass class;
	Coords pos;
	Coords dir;
} Trap;

typedef struct {
	i8 max_hp;
	u8 beat_delay;
	u8 radius;
	bool flying: 1;
	signed dig: 7;
	u32 priority;
	char *glyph;
	void (*act) (Monster*, Coords);
} ClassInfos;

static const ClassInfos class_infos[256];

__extension__
static Tile board[32][32] = {[0 ... 31] = {[0 ... 31] = {.class = WALL, .hp = 5}}};
static const Coords spawn = {23, 9};
static Coords stairs;
static Monster player = {.class = PLAYER, .hp = 1};
static Monster monsters[256];
static Trap traps[256];
static u64 monster_count;

static bool player_moved;
static bool bomb_exploded;
static bool sliding_on_ice;
static bool miniboss_defeated;
static u64 harpies_defeated;
static u32 current_beat;

// Some pre-declarations
static void do_beat(u8 input);
static void damage(Monster *m, i64 dmg, bool bomblike);
static bool forced_move(Monster *m, Coords offset);
