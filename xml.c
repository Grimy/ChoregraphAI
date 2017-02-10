// xml.c - deals with custom dungeon XML files

#include <libxml/xmlreader.h>

#include "chore.h"

// Initial position of the player.
static Coords spawn;

// Returns the numeric value of a named attribute of the current node.
// If the attribute is absent, it defaults to 0.
static i32 xml_attr(xmlTextReader *xml, const char* attr)
{
	char* value = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) (attr));
	i32 result = value ? atoi(value) : 0;
	free(value);
	return result;
}

// Computes the position of the spawn relative to the top-left corner.
// The game uses the spawn as the {0, 0} point, but we use the top-left corner,
// so we need this information to convert between the two reference frames.
// Note: we place {0, 0} on the top-left corner because we want to use
// coordinates to index into the tile array, so they have to be positive.
static void xml_find_spawn(xmlTextReader *xml, __attribute__((unused)) const char *name)
{
	spawn.x = max(spawn.x, 1 - (i8) xml_attr(xml, "x"));
	spawn.y = max(spawn.y, 1 - (i8) xml_attr(xml, "y"));
}

// Converts an item name to an item ID.
static u8 xml_item(xmlTextReader *xml, const char* attr)
{
	static const char* item_names[ITEM_LAST] = {
		[BOMBS]         = "bomb",
		[BOMBS_3]       = "bomb_3",
		[HEART_1]       = "misc_heart_container",
		[HEART_2]       = "misc_heart_container2",
		[JEWELED]       = "weapon_dagger_jeweled",
		[LUNGING]       = "feet_boots_lunging",
		[MEMERS_CAP]    = "head_miners_cap",
		[SCROLL_FREEZE] = "scroll_freeze_enemies",
		[PACEMAKER]     = "heart_transplant",
	};

	char* item_name = (char*) xmlTextReaderGetAttribute(xml, (xmlChar*) attr);

	u8 type = ITEM_LAST;
	while (--type && !streq(item_name, item_names[type]));
	free(item_name);
	return type;
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

static void tile_init(Coords pos, i32 type, i32 zone, bool torch)
{
	static const i8 wall_hp[19] = {1, 1, 5, 0, 4, 4, 0, 2, 3, 5, 4, 0};

	TILE(pos).wired = type == 20 || type == 118;
	TILE(pos).torch = torch;
	if (torch)
		adjust_lights(pos, +1, 4.25);

	if (type >= 100) {
		i8 hp = wall_hp[type - 100];
		TILE(pos).hp = hp;
		type = zone == 4 && (hp == 1 || hp == 2) ? Z4WALL :
			zone == 2 && hp == 2 ? FIREWALL :
			zone == 3 && hp == 2 ? ICEWALL :
			hp ? WALL : DOOR;
	} else if (type == STAIRS) {
		stairs = pos;
		g.locking_enemies = 1 + (zone == 4);
	}

	TILE(pos).type = (u8) type;
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

static void enemy_init(Coords pos, i32 type, bool lord)
{
	static const u8 enemy_id[] = {
		GREEN_SLIME,   // Z1
		SKELETANK_1,   // Z2
		FIRE_SLIME,    // Z3
		BOMBER,        // Z4
		DIREBAT_1,     // Minibosses
		SKELETON_1,    // Debug
		SHOPKEEPER,    // Special
		CRYSTAL_1,     // Z5
	};

	u8 id = enemy_id[type / 100] + type % 100;
	if (id == GHAST || id == GHOUL || id == WRAITH || id == WIGHT)
		return;

	Monster *m = monster_spawn(id, pos, 0);

	if (lord) {
		m->damage *= 2;
		m->hp *= 2;
		m->digging_power *= 2;
		m->flying = true;
	}

	if (id == RED_DRAGON || id == BLUE_DRAGON)
		m->exhausted = 3;
	else if (id == LIGHTSHROOM)
		adjust_lights(pos, +1, 4.5);
	else if (id == ZOMBIE || id == WIRE_ZOMBIE)
		m->dir = orient_zombie(pos);
	else if (m->type == NIGHTMARE_1 || m->type == NIGHTMARE_2)
		g.nightmare = g.last_monster;
}

// Converts a single XML node into an appropriate object (trap, tile, monster or item).
static void xml_process_node(xmlTextReader *xml, const char *name)
{
	i32 type = xml_attr(xml, "type");
	Coords pos = {(i8) xml_attr(xml, "x"), (i8) xml_attr(xml, "y")};

	pos += spawn;
	if (pos.x >= ARRAY_SIZE(g.board) - 1 || pos.y >= ARRAY_SIZE(*g.board) - 1)
		return;

	if (streq(name, "trap"))
		trap_init(pos, type, xml_attr(xml, "subtype"));
	else if (streq(name, "tile"))
		tile_init(pos, type, xml_attr(xml, "zone"), !!xml_attr(xml, "torch"));
	else if (streq(name, "enemy"))
		enemy_init(pos, type, !!xml_attr(xml, "lord"));
	else if (streq(name, "chest"))
		monster_spawn(CHEST, pos, 0)->item = xml_item(xml, "contents");
	else if (streq(name, "crate"))
		monster_spawn(CRATE_2 + (u8) type, pos, 0)->item = xml_item(xml, "contents");
	else if (streq(name, "shrine"))
		monster_spawn(SHRINE, pos, 0);
	else if (streq(name, "item"))
		TILE(pos).item = pos == spawn ? pickup_item(xml_item(xml, "type")) : xml_item(xml, "type");
}

static void xml_process_file(char *file, i64 level, void callback(xmlTextReader *xml, const char *name))
{
	xmlTextReader *xml = xmlReaderForFile(file, NULL, 0);
	if (!xml)
		FATAL("Cannot open file: %s", file);

	while (xmlTextReaderRead(xml) == 1) {
		if (xmlTextReaderNodeType(xml) != 1)
			continue;
		const char* name = (const char*) xmlTextReaderConstName(xml);
		if (streq(name, "dungeon"))
			character = xml_attr(xml, "character") % 1000;
		else if (streq(name, "level"))
			--level;
		else if (!level)
			callback(xml, name);
	}

	if (xmlTextReaderRead(xml) < 0)
		FATAL("Invalid XML file: %s", file);
	xmlFreeTextReader(xml);
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
	if (argc < 2)
		FATAL("Usage: %s dungeon_file.xml [level]", argv[0]);
	i32 level = argc >= 3 ? *argv[2] - '0' : 1;

	g.monsters[0].untrapped = true;

	LIBXML_TEST_VERSION;
	xml_process_file(argv[1], level, xml_find_spawn);
	monster_spawn(PLAYER, spawn, 0);
	xml_process_file(argv[1], level, xml_process_node);

	qsort(g.monsters + 2, g.last_monster - 1, sizeof(Monster), compare_priorities);
	for (u8 i = 1; g.monsters[i].type; ++i) {
		g.monsters[i].aggro = false;
		TILE(g.monsters[i].pos).monster = i;
	}

	assert(player.type == PLAYER);
	if (character == BARD) {
		do_beat('X');
		--g.current_beat;
	} else {
		update_fov();
	}
}
