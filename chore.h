// chore.h - global types and vars definitions

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
	CRATE_1, CRATE_2,
	FREE_SPIDER,
	FIREPIG,

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
typedef enum __attribute__((__packed__)) {
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
} TrapClass;

typedef enum {
	MOVE_FAIL,
	MOVE_SPECIAL,
	MOVE_ATTACK,
	MOVE_SUCCESS,
} MoveResult;

typedef enum {
	DMG_NORMAL,
	DMG_WEAPON,
	DMG_BOMB,
} DamageType;

// A “Monster” is either an enemy or the player. Yes, we are all monsters.
// Honestly, “Entity” is way too generic, and “Character” sounds too much like “char*”.
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
static Coords spawn;
static Coords stairs;
static Monster *nightmare;
static u64 seed = 42;

__extension__
static struct game_state {
	Tile board[35][35];
	Monster _player;
	Monster monsters[64];
	Trap traps[64];

	bool player_moved;
	bool bomb_exploded;
	bool sliding_on_ice;
	bool boots_on;
	bool sarco_on;
	i32 dragon_exhausted;
	i32 miniboss_killed;
	i32 sarcophagus_killed;
	i32 harpies_killed;
	i32 current_beat;
} g = {
	.board = {[0 ... 34] = {[0 ... 34] = {.class = WALL, .hp = 5}}},
	._player = {.class = PLAYER, .hp = 1},
	.boots_on = true,
	.dragon_exhausted = 4,
};

// Some pre-declarations
static void do_beat(u8 input);
static bool damage(Monster *m, i64 dmg, Coords dir, DamageType type);
static bool forced_move(Monster *m, Coords offset);
