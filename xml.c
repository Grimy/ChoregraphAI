// xml.c - deals with custom dungeon XML files

#include <unistd.h>

#include "chore.h"

#define IS_OOB(pos) ((pos).x < 1 || (pos).y < 1 || (pos).x > 30 || (pos).y > 30)

#define HASH(key) (*(key) % 46 % (strlen(key) ^ 13) & 7)
#define STR_ATTR(key) (attribute_map[HASH(key)])
#define INT_ATTR(key) (atoi(STR_ATTR(key)))

// Die verbosely
#define FATAL(message, ...) do { \
	fprintf(stderr, RED message WHITE "\n", __VA_ARGS__); \
	exit(255); } while (0)

const ItemNames item_names[] {
	[NO_ITEM]        = { "",                       "None",                 "" },
	[HEART_1]        = { "misc_heart_container",   "",                     RED "ღ" },
	[HEART_2]        = { "misc_heart_container2",  "",                     RED "ღ" },
	[BOMB_1]         = { "bomb",                   "",                     "●" },
	[BOMB_3]         = { "bomb_3",                 "",                     "●" },
	[SHOVEL_BASE]    = { "shovel_basic",           "Base Shovel",          BROWN "(" },
	[SHOVEL_TIT]     = { "shovel_titanium",        "Titanium Shovel",      "(" },
	[DAGGER_BASE]    = { "weapon_dagger",          "Base Dagger",          ")" },
	[DAGGER_JEWELED] = { "weapon_dagger_jeweled",  "Jeweled Dagger",       BLUE ")" },
	[HEAD_MINERS]    = { "head_miners_cap",        "Miner’s Cap",          BLACK "[" },
	[HEAD_CIRCLET]   = { "head_circlet_telepathy", "Circlet of Telepathy", YELLOW "[" },
	[FEET_LUNGING]   = { "feet_boots_lunging",     "Lunging",              "[" },
	[FEET_LEAPING]   = { "feet_boots_leaping",     "Frog Socks",           GREEN "[" },
	[USE_PACEMAKER]  = { "heart_transplant",       "Heart Transplant",     RED "ღ" },
	[USE_FREEZE]     = { "scroll_freeze_enemies",  "Freeze Scroll",        "?" },
};

// Initial position of the player.
static Coords spawn = {1, 1};

static char attribute_map[8][32];

// Computes the position of the spawn relative to the top-left corner.
// The game uses the spawn as the {0, 0} point, but we use the top-left corner,
// so we need this information to convert between the two reference frames.
// Note: we place {0, 0} on the top-left corner because we want to use
// coordinates to index into the tile array, so they have to be positive.
static void xml_find_spawn(UNUSED const char* node)
{
	if (INT_ATTR("type") >= 100)
		return;
	spawn.x = max(spawn.x, 2 - (i8) INT_ATTR("x"));
	spawn.y = max(spawn.y, 2 - (i8) INT_ATTR("y"));
	if (IS_OOB(spawn))
		FATAL("Tile too far away from spawn: (%d, %d)", INT_ATTR("x"), INT_ATTR("y"));
}

// Converts an item name to an item ID.
static u8 xml_item(const char* key)
{
	u8 item = ARRAY_SIZE(item_names);
	while (--item && !streq(STR_ATTR(key), item_names[item].xml));
	return item;
}

// Adds a new trap to the list.
static void trap_init(Coords pos, i32 type, i32 subtype)
{
	static u64 trap_count;

	if (type == 10) {
		monster_spawn(FIREPIG, pos, 0)->dir.x = subtype ? -1 : 1;
		return;
	}

	Trap *trap = &g.traps[trap_count++];
	trap->type = subtype == 8 ? OMNIBOUNCE : (u8) type;
	trap->pos = pos;
	trap->dir.x = (i8[]) {1, -1, 0, 0, 1, -1, -1, 1} [subtype & 7];
	trap->dir.y = (i8[]) {0, 0, 1, -1, 1, 1, -1, -1} [subtype & 7];
}

static TileType tile_types[] = {
	FLOOR, FLOOR, STAIRS, SHOP_FLOOR, WATER, TAR, NONE, SHOP_FLOOR, TAR, STAIRS,
	FIRE, ICE, NONE, NONE, FLOOR, NONE, NONE, OOZE, LAVA, FLOOR, FLOOR,
	[21 ... 99] = NONE,
	DIRT, DIRT, NONE, DOOR, SHOP, NONE, NONE, STONE, CATACOMB, NONE, SHOP,
	DOOR, SHOP, SHOP, SHOP, SHOP, SHOP, SHOP, DOOR,
};

static void tile_init(Coords pos, i32 type, i32 zone, bool torch)
{
	if (type > ARRAY_SIZE(tile_types) || tile_types[type] == NONE)
		FATAL("Unknown tile type: %d", type);

	u8 id = tile_types[type];
	if (id == STONE && (zone == 2 || zone == 3))
		id |= zone == 2 ? ICE : FIRE;
	if ((id == DIRT || id == STONE) && zone == 4)
		id |= 8;

	if (torch)
		adjust_lights(pos, +1, 4.25);

	if (id == STAIRS) {
		g.stairs = pos;
		g.locking_enemies = 1 + (zone == 4);
	}

	TILE(pos).wired = type == 20 || type == 118;
	TILE(pos).torch = torch;
	TILE(pos).type = id;
}

// Guesses at a zombie’s initial orientation.
// Unfortunately, this information isn’t present in dungeon files.
static Coords orient_zombie(Coords pos)
{
	for (int i = 3; i >= 0; --i)
		if (IS_WIRE(pos + plus_shape[i]))
			return plus_shape[i];
	return {1, 0};
}

static MonsterType monster_types[] = {
	GREEN_SLIME, BLUE_SLIME, YELLOW_SLIME, SKELETON_1, SKELETON_2, SKELETON_3,
	BLUE_BAT, RED_BAT, GREEN_BAT, MONKEY_1, MONKEY_2, GHOST, ZOMBIE, WRAITH,
	MIMIC_1, MIMIC_2, WHITE_MIMIC,
	[100] = SKELETANK_1, SKELETANK_2, SKELETANK_3, WINDMAGE_1, WINDMAGE_2,
	WINDMAGE_3, MUSHROOM_1, MUSHROOM_2, GOLEM_1, GOLEM_2, ARMADILLO_1, ARMADILLO_2,
	CLONE, TARMONSTER, MOLE, WIGHT, WALL_MIMIC, LIGHTSHROOM, BOMBSHROOM,
	[200] = FIRE_SLIME, ICE_SLIME, RIDER_1, RIDER_2, RIDER_3, EFREET, DJINN,
	ASSASSIN_1, ASSASSIN_2, FIRE_BEETLE, ICE_BEETLE, HELLHOUND, SHOVE_1,
	YETI, GHAST, FIRE_MIMIC, ICE_MIMIC, FIRE_POT, ICE_POT, SHOVE_2,
	[300] = BOMBER, DIGGER, BLACK_BAT, ARMADILDO, BLADENOVICE, BLADEMASTER,
	GHOUL, GOOLEM, HARPY, LICH_1, LICH_2, LICH_3, CONF_MONKEY, TELE_MONKEY, PIXIE,
	SARCO_1, SARCO_2, SARCO_3, SPIDER, WARLOCK_1, WARLOCK_2, MUMMY, WIND_STATUE,
	MIMIC_STATUE, BOMB_STATUE, MINE_STATUE, CRATE, CRATE,
	[400] = DIREBAT_1, DIREBAT_2, DRAGON, RED_DRAGON, BLUE_DRAGON,
	BANSHEE_1, BANSHEE_2, MINOTAUR_1, MINOTAUR_2, NIGHTMARE_1, NIGHTMARE_2,
	MOMMY, OGRE, METROGNOME_1, METROGNOME_2, EARTH_DRAGON,
	[600] = SHOPKEEPER,
	[701] = SKULL_1, WATER_BALL, NO_MONSTER, ELECTRO_1, ELECTRO_2, ELECTRO_3,
	ORB_1, ORB_2, ORB_3, GORGON_1, WIRE_ZOMBIE, SKULL_2, SKULL_3, NO_MONSTER,
	NO_MONSTER, NO_MONSTER, EVIL_EYE_1, GORGON_2, EVIL_EYE_2, ORC_1, ORC_2, ORC_3,
	DEVIL_1, DEVIL_2, PURPLE_SLIME, CURSE_WRAITH, MIMIC_1, SHOP_MIMIC,
	BLACK_SLIME, NO_MONSTER, MIMIC_1, MIMIC_1, TAR_BALL,
};

static void enemy_init(Coords pos, i32 type, bool lord)
{
	if (type > ARRAY_SIZE(monster_types) || monster_types[type] == NO_MONSTER)
		FATAL("Invalid enemy type: %d", type);

	MonsterType id = monster_types[type];
	if (id >= GHAST && id <= CURSE_WRAITH)
		return;

	Monster *m = monster_spawn(id, pos, 0);

	if (lord) {
		m->damage *= 2;
		m->hp *= 2;
		m->digging_power = m->digging_power == NONE ? NONE : SHOP;
		m->flying = true;
	}

	if (id == RED_DRAGON || id == BLUE_DRAGON)
		m->exhausted = 3;
	else if (id == LIGHTSHROOM)
		adjust_lights(pos, +1, 4.5);
	else if (id == ZOMBIE || id == WIRE_ZOMBIE)
		m->dir = orient_zombie(pos);
}

// Converts a single XML node into an appropriate object (trap, tile, monster or item).
static void xml_process_node(const char *name)
{
	i32 type = INT_ATTR("type");
	if (type < 0)
		FATAL("Invalid %s type: %d", name, type);

	Coords pos = Coords {(i8) INT_ATTR("x"), (i8) INT_ATTR("y")} + spawn;
	if (IS_OOB(pos)) {
		if (type >= 100)
			return;
		FATAL("Out of bounds entity: (%d, %d)", INT_ATTR("x"), INT_ATTR("y"));
	}

	if (streq(name, "trap"))
		trap_init(pos, type, INT_ATTR("subtype"));
	else if (streq(name, "tile"))
		tile_init(pos, type, INT_ATTR("zone"), INT_ATTR("torch"));
	else if (streq(name, "enemy"))
		enemy_init(pos, type, INT_ATTR("lord"));
	else if (streq(name, "chest"))
		monster_spawn(CHEST, pos, 0)->item = xml_item("contents");
	else if (streq(name, "crate"))
		monster_spawn(CRATE + (u8) type, pos, 0)->item = xml_item("contents");
	else if (streq(name, "shrine"))
		monster_spawn(SHRINE, pos, 0);
	else if (streq(name, "item") && pos == spawn)
		pickup_item(xml_item("type"));
	else if (streq(name, "item"))
		TILE(pos).item = xml_item("type");
}

static void dungeon_init(i32 level)
{
	g.character = (CharId) (INT_ATTR("character") % 1000);
	if (level > INT_ATTR("numLevels"))
		FATAL("No level %d in dungeon (max: %d)", level, INT_ATTR("numLevels"));
}

static void xml_process_file(char *file, i32 level, void (callback)(const char*))
{
	FILE *xml = fopen(file, "r");
	char node[16];
	char key[16];
	bool ok = false;

	if (!xml)
		FATAL("Cannot open file: %s", file);

	while (fscanf(xml, " <%15[?/a-z]", node) > 0) {
		memset(attribute_map, 0, sizeof(attribute_map));
		while (fscanf(xml, " %12[a-zA-Z]", key) > 0)
			fscanf(xml, " = \"%31[^\"]\"", STR_ATTR(key));
		fscanf(xml, " %*[?/>]");

		if (streq(node, "dungeon"))
			dungeon_init(level);
		else if (streq(node, "level"))
			ok = INT_ATTR("num") == level;
		else if (ok && (INT_ATTR("x") > -180 || INT_ATTR("y") > -180))
			callback(node);
	}

	if (!streq(node, "/dungeon") || fscanf(xml, "%*c") != EOF)
		FATAL("File isn’t valid dungeon XML: %s", file);
}

// Compares the priorities of two monsters. Callback for qsort.
static i32 compare_priorities(const void *a, const void *b)
{
	u64 pa = ((const Monster*) a)->priority;
	u64 pb = ((const Monster*) b)->priority;
	return (pa > pb) - (pa < pb);
}

// Initializes the game’s state based on the given custom dungeon file.
// Aborts if the file doesn’t exist or isn’t valid XML.
// Valid, non-dungeon XML yields undefined results (most likely, an empty dungeon).
void xml_parse(i32 argc, char **argv)
{
	i32 opt;
	i32 level = 1;
	u8 item = ARRAY_SIZE(item_names);

	while ((opt = getopt(argc, argv, "i:l:m:s:")) != -1) {
		switch (opt) {
		case 'i':
			while (--item && !strstr(item_names[item].xml, optarg));
			pickup_item(item);
			break;
		case 'l':
			level = atoi(optarg);
			break;
		case 'm':
			strncpy(g.input, optarg, sizeof(g.input));
			break;
		case 's':
			g.seed = (u32) atoi(optarg);
			break;
		case '?':
			exit(255);
		}
	}

	if (level <= 0)
		FATAL("Invalid level: %s (expected a positive integer)", argv[2]);

	if (argc <= optind)
		FATAL("Usage: %s [options] dungeon_file.xml", argv[0]);

	xml_process_file(argv[optind], level, xml_find_spawn);
	monster_spawn(PLAYER, spawn, 0);
	xml_process_file(argv[optind], level, xml_process_node);

	if (MONSTER(spawn).type != PLAYER)
		FATAL("Non-player entity at spawn: %s", TYPE(&MONSTER(spawn)).glyph);

	qsort(g.monsters + 2, g.last_monster - 1, sizeof(Monster), compare_priorities);
	for (u8 i = 1; g.monsters[i].type; ++i) {
		Monster &m = g.monsters[i];
		m.aggro = false;
		TILE(m.pos).monster = i;
		if (m.type == NIGHTMARE_1 || m.type == NIGHTMARE_2)
			g.nightmare = i;
	}

	if (g.character == BARD)
		g.input[0] = 'X';
	else
		update_fov();

	for (char *p = g.input; *p; ++p)
		do_beat(*p);

	assert(player.type == PLAYER);
	g.current_beat = 0;
}
