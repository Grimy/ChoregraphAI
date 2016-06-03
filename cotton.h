// cotton.h - types and global vars definitions

// Boolean type.
typedef enum {false, true} bool;

// A pair of cartesian coordinates.
typedef int8_t Coords __attribute__((ext_vector_type(2)));

// Human-friendly names for monster classes.
// Numerical values were arbitrarily picked to make parsing dungeon XML easier.
typedef enum __attribute__((__packed__)) {
	GREEN_SLIME, BLUE_SLIME, YOLO_SLIME,
	SKELETON_1, SKELETON_2, SKELETON_3,
	BLUE_BAT, RED_BAT, GREEN_BAT,
	MONKEY_1, MONKEY_2,
	GHOST,
	ZOMBIE,
	WRAITH,
	MIMIC_1, MIMIC_2, MIMIC_3,

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

	BOMBER = 44,
	DIGGER,
	BLACK_BAT,
	ARMADILDO,
	BLADENOVICE, BLADEMASTER,
	GHOUL,
	OOZE_GOLEM,
	HARPY,
	LICH_1, LICH_2, LICH_3,
	CONF_MONKEY,
	TELE_MONKEY,
	PIXIE,
	SARCO_1, SARCO_2, SARCO_3,
	SPIDER,
	WARLOCK_1, WARLOCK_2,
	MUMMY,
	GARGOYLE_1, GARGOYLE_2, GARGOYLE_3, GARGOYLE_4, GARGOYLE_5, GARGOYLE_6,
	FREE_SPIDER,

	SHOPKEEPER = 88,

	DIREBAT_1 = 144, DIREBAT_2,
	DRAGON, RED_DRAGON, BLUE_DRAGON,
	BANSHEE_1, BANSHEE_2,
	MINOTAUR_1, MINOTAUR_2,
	NIGHTMARE_1, NIGHTMARE_2,
	MOMMY, OGRE,

	PLAYER,
	TEST = 255,
} MonsterClass;

// Human-readable names for tile types.
// Note that WALL can be any wall type, including level edges and doors.
typedef enum __attribute__((__packed__)) {
	WALL = 0,
	FLOOR = 1,
	WATER = 2,
	TAR = 8,
	STAIRS = 9,
	FIRE = 10,
	ICE = 11,
	OOZE = 17,
} TileClass;

// Human-readable names for traps.
// BOUNCE includes all eight directional bounce traps, but not omni-bounce nor random-bounce.
typedef enum __attribute__((__packed__)) {
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
typedef struct {
	MonsterClass class;
	int8_t hp;
	Coords pos;
	Coords prev_pos;
	unsigned delay: 4;
	bool aggro: 1;
	bool vertical: 1;
	unsigned state: 2;
	unsigned confused: 4;
	unsigned: 4;
} Monster;

typedef struct {
	TileClass class;
	int8_t hp;
	int8_t torch;
	int8_t zone;
	int8_t revealed;
	unsigned: 24;
	Monster *monster;
} Tile;

typedef struct {
	Coords pos;
	Coords dir;
	TrapClass class;
	unsigned: 24;
} Trap;

typedef struct {
	int8_t max_hp;
	uint8_t beat_delay;
	uint8_t radius;
	bool flying: 1;
	unsigned dig: 7;
	uint32_t priority;
	char *glyph;
	void (*act) (Monster*, Coords);
} ClassInfos;

static const ClassInfos class_infos[256];

__extension__
static Tile board[32][32] = {[0 ... 31] = {[0 ... 31] = {.class = WALL, .hp = 5}}};
static Monster player = {.class = PLAYER, .hp = 1, .pos = {SPAWN_X, SPAWN_Y}};
static Monster monsters[256];
static Trap traps[256];
static uint64_t monster_count = 0;
